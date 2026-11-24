#include "rooms/slot_solver.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>

#include <ortools/sat/cp_model.h>
#include <ortools/sat/cp_model.pb.h>
#include <ortools/sat/cp_model_solver.h>
#include <ortools/sat/model.h>
#include <ortools/sat/sat_parameters.pb.h>

#include "problem_data.hpp"
#include "rooms/room_assignment.hpp"
#include "utils/jagged_array.hpp"
#include "utils/span.hpp"
#include "utils/vector.hpp"

namespace cetp::rooms {
namespace {

namespace sat = operations_research::sat;

constexpr int SINGLE_SEARCH_WORKER = 1;
constexpr int FULL_LINEARIZATION_LEVEL = 2;
constexpr int UNSET_LOCATION = -1;

// Pairs a candidate room ID with the 0/1 variable deciding whether a section uses that room.
struct RoomUse {
  int room = 0;
  sat::BoolVar choice;
};

// Pairs a campus ID with a section's 0/1 indicator that the section is placed at that campus.
struct LocationIndicator {
  int location = 0;
  sat::BoolVar indicator;
};

[[nodiscard]] auto map_solver_status(sat::CpSolverStatus status) noexcept -> RoomAssignmentStatus {
  switch (status) {
  case sat::CpSolverStatus::OPTIMAL:
    return RoomAssignmentStatus::OPTIMAL;
  case sat::CpSolverStatus::FEASIBLE:
    return RoomAssignmentStatus::FEASIBLE;
  case sat::CpSolverStatus::INFEASIBLE:
    return RoomAssignmentStatus::INFEASIBLE;
  default:
    return RoomAssignmentStatus::UNKNOWN;
  }
}

// Formulates and solves room allocation for one independent time slot as a 0-1 CP-SAT model:
// - Hard constraints: enough seats per section, one campus per section, <= 1 section per room.
// - Lexicographic objective (encoded via exact integer weights):
//     1. Minimize total opened rooms, 2. Minimize exam campus splits, 3. Minimize unused seats.
class SlotSolver {
public:
  SlotSolver(const ProblemData& problem, Span<const int> slot_exams)
      : problem_(problem),
        slot_exams_(slot_exams),
        section_location_indices_(problem.num_locations, UNSET_LOCATION) {}

  [[nodiscard]] auto solve(double time_limit, int solver_seed) -> SlotSolution {
    build_model();

    sat::SatParameters sat_params;
    sat_params.set_max_time_in_seconds(time_limit);
    // Use 1 internal worker so CP-SAT stays deterministic; parallelism runs across slots in OpenMP.
    sat_params.set_num_search_workers(SINGLE_SEARCH_WORKER);
    // Single-worker CP-SAT defaults to linearization_level=1, which leaves boolean implications
    // unlinearized in the LP relaxation; level 2 restores a tight LP bound on 0-1 room models.
    sat_params.set_linearization_level(FULL_LINEARIZATION_LEVEL);
    sat_params.set_random_seed(solver_seed);

    sat::Model solver;
    solver.Add(sat::NewSatParameters(sat_params));
    const sat::CpSolverResponse response = sat::SolveCpModel(model_.Build(), &solver);

    assert(response.status() != sat::CpSolverStatus::MODEL_INVALID);
    const RoomAssignmentStatus status = map_solver_status(response.status());
    if (!is_found(status)) {
      return {.status = status};
    }
    return extract_solution(response, status);
  }

private:
  void build_model() {
    for (int exam : slot_exams_) {
      add_exam_constraints(exam);
    }
    const int64_t max_slot_capacity = add_exclusive_room_constraints();
    set_weighted_objective(max_slot_capacity);
  }

  // Adds per-section constraints for an exam and, if it has 2+ sections, penalizes campus splits.
  void add_exam_constraints(int exam) {
    const auto exam_sections = problem_.sections_of_exam(exam);
    const int section_count = int(exam_sections.size());
    const bool multi_section = section_count > 1;
    if (multi_section) {
      exam_location_indicators_.clear();
    }
    for (int section : exam_sections) {
      add_section_constraints(section, multi_section);
    }
    if (multi_section) {
      add_exam_location_penalty(section_count);
    }
  }

  void add_section_constraints(int section, bool track_exam_locations) {
    section_location_variables_.clear();
    std::ranges::fill(section_location_indices_, UNSET_LOCATION);

    sat::LinearExpr section_capacity;
    sat::LinearExpr section_rooms;
    int max_room_capacity = 0;

    // Create one 0/1 decision variable per eligible room: `choice == 1` iff this section uses it.
    for (int room : problem_.feasible_rooms_by_section[section]) {
      const sat::BoolVar choice = model_.NewBoolVar();
      room_choices_.push_back(choice);
      room_uses_.push_back({.room = room, .choice = choice});

      const int seats = problem_.room_capacities[room];
      max_room_capacity = std::max(max_room_capacity, seats);
      section_capacity += seats * choice;
      section_rooms += choice;
      total_capacity_ += seats * choice;
      room_count_ += choice;

      // Lazily create one 0/1 variable per campus reachable by this section.
      // `AddImplication(choice, location_variable)` forces the campus flag to 1 when room is used.
      const int location = problem_.room_location_ids[room];
      int& location_index = section_location_indices_[location];
      if (location_index == UNSET_LOCATION) {
        location_index = section_location_variables_.size();
        const sat::BoolVar location_variable = model_.NewBoolVar();
        section_location_variables_.push_back(location_variable);
        if (track_exam_locations) {
          exam_location_indicators_.push_back(
              {.location = location, .indicator = location_variable}
          );
        }
      }
      model_.AddImplication(choice, section_location_variables_[location_index]);
    }

    // Hard section constraints:
    // - AddExactlyOne: sum(campus flags) == 1, keeping all rooms of this section on one campus.
    // - section_rooms >= ceil(students / max_room_capacity): valid cut tightening LP lower bounds.
    // - section_capacity >= students: selected rooms must seat every student in the section.
    const int students = problem_.section_student_counts[section];
    const int min_rooms = ((students - 1) / max_room_capacity) + 1;
    model_.AddExactlyOne(section_location_variables_.std_vector());
    model_.AddGreaterOrEqual(section_rooms, min_rooms);
    model_.AddGreaterOrEqual(section_capacity, students);
  }

  // Penalizes distinct campuses spanned by a multi-section exam.
  // For each campus shared by 2+ sections, creates `exam_location` with
  // `indicator => exam_location` (no reverse implication is needed because minimizing
  // `used_locations_` drives unused flags to 0). Since each exam uses >= 1 campus, minimizing
  // `used_locations_` equals minimizing `extra_locations` (= distinct_campuses - 1 per exam).
  void add_exam_location_penalty(int section_count) {
    assert(!exam_location_indicators_.empty());
    std::ranges::stable_sort(exam_location_indicators_, {}, &LocationIndicator::location);
    if (exam_location_indicators_.front().location == exam_location_indicators_.back().location) {
      return;
    }

    int eligible_locations = 0;
    for (int begin = 0; begin < exam_location_indicators_.size();) {
      const int location = exam_location_indicators_[begin].location;
      int end = begin + 1;
      while (end < exam_location_indicators_.size()
             && exam_location_indicators_[end].location == location) {
        ++end;
      }
      if (end - begin == 1) {
        // Only one section can use this campus, so its section flag directly serves as exam flag.
        used_locations_ += exam_location_indicators_[begin].indicator;
      } else {
        const sat::BoolVar exam_location = model_.NewBoolVar();
        for (int index = begin; index < end; ++index) {
          model_.AddImplication(exam_location_indicators_[index].indicator, exam_location);
        }
        used_locations_ += exam_location;
      }
      ++eligible_locations;
      begin = end;
    }
    // An exam can span at most `min(section_count, eligible_locations)` campuses (-1 for baseline).
    max_extra_locations_ += std::min(section_count, eligible_locations) - 1;
  }

  // Enforces hard room exclusivity: each room serves at most one section in this slot.
  // Sorting `room_uses_` by room ID groups all sections competing for the same room so we can add
  // `AddAtMostOne` (`sum(choices) <= 1`). Returns total seat capacity of all candidate rooms.
  [[nodiscard]] auto add_exclusive_room_constraints() -> int64_t {
    std::ranges::stable_sort(room_uses_, {}, &RoomUse::room);

    int64_t max_slot_capacity = 0;
    Vector<sat::BoolVar> room_group;
    for (int begin = 0; begin < room_uses_.size();) {
      const int room = room_uses_[begin].room;
      room_group.clear();
      int end = begin;
      while (end < room_uses_.size() && room_uses_[end].room == room) {
        room_group.push_back(room_uses_[end].choice);
        ++end;
      }
      if (room_group.size() > 1) {
        model_.AddAtMostOne(room_group.std_vector());
      }
      max_slot_capacity += problem_.room_capacities[room];
      begin = end;
    }
    return max_slot_capacity;
  }

  // Encodes strict 3-level lexicographic priority into one linear objective (like place values):
  // - `location_weight = max_slot_capacity + 1`: saving 1 campus split beats any seat-waste delta.
  // - `room_weight = location_weight * (max_extra_locations_ + 1)`: saving 1 room beats the worst
  //   combined penalty of campus splits and seat capacity.
  // Minimizing `total_capacity_` is equivalent to minimizing `unused_capacity` (demand is fixed).
  void set_weighted_objective(int64_t max_slot_capacity) {
    const int64_t location_weight = max_slot_capacity + 1;
    const int64_t room_weight = location_weight * (max_extra_locations_ + 1);
    model_.Minimize(
        room_count_ * room_weight + used_locations_ * location_weight + total_capacity_
    );
  }

  // Reads chosen rooms from `response` in the exact order `room_choices_` was populated
  // (slot_exams_ -> sections_of_exam -> feasible_rooms_by_section) and recomputes
  // `extra_locations` and `unused_capacity` directly from chosen rooms (exact even on FEASIBLE).
  [[nodiscard]] auto extract_solution(
      const sat::CpSolverResponse& response,
      RoomAssignmentStatus status
  ) const -> SlotSolution {
    SlotSolution solution;
    solution.status = status;
    // Each room can be opened at most once per slot, bounding total assigned room entries.
    solution.rooms_by_local_section.reserve_num_elements(
        std::min(problem_.num_rooms, room_choices_.size())
    );

    Vector<bool> used_locations(problem_.num_locations);
    int choice_index = 0;
    for (int exam : slot_exams_) {
      std::ranges::fill(used_locations, false);
      int distinct_locations = 0;
      for (int section : problem_.sections_of_exam(exam)) {
        int section_location = -1;
        for (int room : problem_.feasible_rooms_by_section[section]) {
          if (!sat::SolutionBooleanValue(response, room_choices_[choice_index++])) {
            continue;
          }
          solution.rooms_by_local_section.append(room);
          solution.unused_capacity += problem_.room_capacities[room];
          section_location = problem_.room_location_ids[room];
        }
        assert(section_location >= 0);
        solution.unused_capacity -= problem_.section_student_counts[section];
        solution.rooms_by_local_section.finish_row();
        if (!used_locations[section_location]) {
          used_locations[section_location] = true;
          ++distinct_locations;
        }
      }
      solution.extra_locations += distinct_locations - 1;
    }
    return solution;
  }

  // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
  const ProblemData& problem_;
  Span<const int> slot_exams_;
  sat::CpModelBuilder model_;
  // All (section, room) BoolVars in traversal order, read back by `extract_solution`.
  Vector<sat::BoolVar> room_choices_;
  Vector<RoomUse> room_uses_;

  // Linear expressions combined into the weighted lexicographic objective.
  sat::LinearExpr room_count_;
  sat::LinearExpr total_capacity_;
  sat::LinearExpr used_locations_;
  int64_t max_extra_locations_ = 0;

  // Reusable per-section / per-exam scratch buffers to avoid repeated allocations.
  Vector<int> section_location_indices_;
  Vector<sat::BoolVar> section_location_variables_;
  Vector<LocationIndicator> exam_location_indicators_;
};

}  // namespace

auto solve_slot(
    const ProblemData& problem,
    Span<const int> slot_exams,
    double time_limit,
    int solver_seed
) -> SlotSolution {
  return SlotSolver(problem, slot_exams).solve(time_limit, solver_seed);
}

}  // namespace cetp::rooms
