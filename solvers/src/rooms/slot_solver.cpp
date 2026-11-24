#include "rooms/slot_solver.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <utility>

#include <ortools/sat/cp_model.h>
#include <ortools/sat/cp_model.pb.h>
#include <ortools/sat/cp_model_solver.h>
#include <ortools/sat/model.h>
#include <ortools/sat/sat_parameters.pb.h>

#include "problem_data.hpp"
#include "rooms/room_assignment.hpp"
#include "rooms/slot_model.hpp"
#include "utils/span.hpp"
#include "utils/vector.hpp"

namespace cetp::rooms {
namespace {

namespace sat = operations_research::sat;
using Status = RoomAssignmentStatus;

// Deterministic 31-bit non-negative mask for OR-Tools SatParameters::random_seed (int32).
constexpr uint64_t SEED_MASK = 0x7fffffffULL;

auto elapsed_seconds(SteadyClock::time_point start_time) -> double {
  return std::chrono::duration<double>(SteadyClock::now() - start_time).count();
}

auto has_solution(const sat::CpSolverResponse& response) -> bool {
  return response.status() == sat::CpSolverStatus::OPTIMAL
         || response.status() == sat::CpSolverStatus::FEASIBLE;
}

auto failure_status(const sat::CpSolverResponse& response) -> Status {
  switch (response.status()) {
  case sat::CpSolverStatus::INFEASIBLE:
    return Status::INFEASIBLE;
  case sat::CpSolverStatus::MODEL_INVALID:
    return Status::MODEL_INVALID;
  default:
    return Status::UNKNOWN;
  }
}

auto collect_slot_sections(const ProblemData& problem, Span<const int> slot_exams) -> Vector<int> {
  Vector<int> slot_sections;
  for (int exam : slot_exams) {
    for (int section : problem.sections_of_exam(exam)) {
      slot_sections.push_back(section);
    }
  }
  return slot_sections;
}

// Encapsulates the 3-phase lexicographic CP-SAT solve for a single time slot.
// Running separate phases guarantees strict priority without Big-M weights that could overflow
// or weaken LP relaxations inside CP-SAT.
class SlotSolver {
public:
  explicit SlotSolver(const SlotSolveRequest& request)
      : request_(request),
        slot_sections_(collect_slot_sections(request.problem, request.slot_exams)),
        slot_model_(build_slot_model(request.problem, request.slot_exams, slot_sections_)),
        slot_started_(SteadyClock::now()),
        slot_seed_(int((uint64_t(request.base_seed) + uint64_t(request.slot)) & SEED_MASK)) {}

  [[nodiscard]] auto solve() -> SlotSolution {
    // Phase 1: Minimize total number of rooms opened in this slot.
    slot_model_.model.Minimize(slot_model_.room_count);
    if (remaining_seconds() <= 0.0) {
      mark_failed();
      return {.status = Status::UNKNOWN};
    }

    best_response_ = run_phase();
    if (!has_solution(best_response_)) {
      mark_failed();
      return {.status = failure_status(best_response_)};
    }

    // If another slot already failed, overall assignment will be cleared; skip phases 2 & 3 so
    // we still finish reporting every failing slot quickly.
    if (request_.any_slot_failed.load(std::memory_order_relaxed)) {
      return {.status = Status::FEASIBLE};
    }

    const Status final_status = run_secondary_phases();
    if (final_status == Status::MODEL_INVALID) {
      mark_failed();
      return {.status = Status::MODEL_INVALID};
    }
    return extract_solution(final_status);
  }

private:
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
  const SlotSolveRequest& request_;
  Vector<int> slot_sections_;
  SlotModel slot_model_;
  SteadyClock::time_point slot_started_;
  int slot_seed_ = 0;
  sat::CpSolverResponse best_response_;

  void mark_failed() const noexcept {
    request_.any_slot_failed.store(true, std::memory_order_relaxed);
  }

  [[nodiscard]] auto remaining_seconds() const -> double {
    const double global_remaining =
        request_.global_time_limit - elapsed_seconds(request_.global_started);
    const double slot_remaining = request_.slot_time_limit - elapsed_seconds(slot_started_);
    return std::min(global_remaining, slot_remaining);
  }

  // Feeds the previous phase's solution back into CP-SAT as a "warm start" hint so the next
  // phase immediately starts from a valid feasible assignment.
  void set_solution_hints() {
    slot_model_.model.ClearHints();
    for (const sat::BoolVar variable : slot_model_.all_variables) {
      slot_model_.model.AddHint(variable, sat::SolutionBooleanValue(best_response_, variable));
    }
  }

  // Runs CP-SAT with 1 internal worker and a deterministic per-slot seed so parallel OpenMP
  // execution across slots produces identical results across runs.
  [[nodiscard]] auto run_phase() const -> sat::CpSolverResponse {
    sat::SatParameters solver_parameters;
    solver_parameters.set_max_time_in_seconds(std::max(0.0, remaining_seconds()));
    solver_parameters.set_num_search_workers(1);
    solver_parameters.set_random_seed(slot_seed_);
    sat::Model solver;
    solver.Add(sat::NewSatParameters(solver_parameters));
    return sat::SolveCpModel(slot_model_.model.Build(), &solver);
  }

  [[nodiscard]] auto run_secondary_phases() -> Status {
    if (best_response_.status() != sat::CpSolverStatus::OPTIMAL || remaining_seconds() <= 0.0) {
      return Status::FEASIBLE;
    }

    const Status phase2_status = optimize_extra_locations();
    if (phase2_status != Status::OPTIMAL) {
      return phase2_status;
    }
    if (request_.any_slot_failed.load(std::memory_order_relaxed) || remaining_seconds() <= 0.0) {
      return Status::FEASIBLE;
    }
    return optimize_unused_capacity();
  }

  // Phase 2: Lock minimal `room_count`, then minimize `extra_locations` (exam campus splits).
  [[nodiscard]] auto optimize_extra_locations() -> Status {
    const int64_t minimum_rooms = sat::SolutionIntegerValue(best_response_, slot_model_.room_count);
    slot_model_.model.AddEquality(slot_model_.room_count, minimum_rooms);

    const int64_t incumbent_extra_locations =
        sat::SolutionIntegerValue(best_response_, slot_model_.extra_locations);
    if (!slot_model_.has_variable_exam_locations
        || incumbent_extra_locations <= slot_model_.extra_locations_lower_bound) {
      // Phase 1 already achieved the theoretical lower bound (usually 0): skip phase 2 solve.
      if (slot_model_.has_variable_exam_locations) {
        slot_model_.model.AddEquality(slot_model_.extra_locations, incumbent_extra_locations);
      }
      return Status::OPTIMAL;
    }

    slot_model_.model.Minimize(slot_model_.extra_locations);
    set_solution_hints();
    auto phase2_response = run_phase();
    if (phase2_response.status() == sat::CpSolverStatus::MODEL_INVALID) {
      return Status::MODEL_INVALID;
    }
    if (!has_solution(phase2_response)) {
      return Status::FEASIBLE;
    }

    const bool proved_optimal = phase2_response.status() == sat::CpSolverStatus::OPTIMAL;
    if (sat::SolutionIntegerValue(phase2_response, slot_model_.extra_locations)
        <= incumbent_extra_locations) {
      best_response_ = std::move(phase2_response);
    }
    if (!proved_optimal) {
      return Status::FEASIBLE;
    }

    const int64_t minimum_extra_locations =
        sat::SolutionIntegerValue(best_response_, slot_model_.extra_locations);
    slot_model_.model.AddEquality(slot_model_.extra_locations, minimum_extra_locations);
    return Status::OPTIMAL;
  }

  // Phase 3: Lock minimal `room_count` & `extra_locations`, then minimize `total_capacity`.
  [[nodiscard]] auto optimize_unused_capacity() -> Status {
    slot_model_.model.Minimize(slot_model_.total_capacity);
    set_solution_hints();
    auto phase3_response = run_phase();
    if (phase3_response.status() == sat::CpSolverStatus::MODEL_INVALID) {
      return Status::MODEL_INVALID;
    }
    if (!has_solution(phase3_response)) {
      return Status::FEASIBLE;
    }

    const Status phase3_status = phase3_response.status() == sat::CpSolverStatus::OPTIMAL
                                     ? Status::OPTIMAL
                                     : Status::FEASIBLE;
    if (sat::SolutionIntegerValue(phase3_response, slot_model_.total_capacity)
        <= sat::SolutionIntegerValue(best_response_, slot_model_.total_capacity)) {
      best_response_ = std::move(phase3_response);
    }
    return phase3_status;
  }

  [[nodiscard]] auto extract_solution(Status status) const -> SlotSolution {
    SlotSolution solution;
    solution.status = status;
    solution.rooms_by_local_section.reserve_num_rows(slot_sections_.size());
    solution.rooms_by_local_section.reserve_num_elements(slot_model_.room_choices.size());
    solution.extra_locations =
        sat::SolutionIntegerValue(best_response_, slot_model_.extra_locations);

    int choice_index = 0;
    for (int section : slot_sections_) {
      for (int room : request_.problem.feasible_rooms_by_section[section].values) {
        if (sat::SolutionBooleanValue(best_response_, slot_model_.room_choices[choice_index])) {
          solution.rooms_by_local_section.append(room);
          solution.unused_capacity += request_.problem.room_capacities[room];
        }
        ++choice_index;
      }
      solution.unused_capacity -= request_.problem.section_student_counts[section];
      solution.rooms_by_local_section.finish_row();
    }
    return solution;
  }
};

}  // namespace

auto solve_single_slot(const SlotSolveRequest& request) -> SlotSolution {
  return SlotSolver(request).solve();
}

}  // namespace cetp::rooms
