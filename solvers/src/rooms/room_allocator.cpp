#include "rooms/room_allocator.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <tuple>
#include <utility>

#include <ortools/sat/cp_model.h>
#include <ortools/sat/cp_model.pb.h>
#include <ortools/sat/cp_model_solver.h>
#include <ortools/sat/model.h>
#include <ortools/sat/sat_parameters.pb.h>

#include "problem_data.hpp"
#include "rooms/params.hpp"
#include "rooms/room_assignment.hpp"
#include "timetable.hpp"
#include "utils/csr_matrix.hpp"
#include "utils/vector.hpp"

namespace cetp::rooms {
namespace {

namespace sat = operations_research::sat;
using Clock = std::chrono::steady_clock;
using Status = RoomAssignmentStatus;

struct RoomUse {
  int slot;
  int room;
  int choice;
};

// Sparse grouping by (slot, room) avoids a slots x rooms x sections structure.
void add_exclusive_room_constraints(
    sat::CpModelBuilder& model,
    const Vector<sat::BoolVar>& choices,
    Vector<RoomUse>& uses
) {
  std::ranges::sort(uses, [](const RoomUse& left, const RoomUse& right) -> bool {
    return std::tie(left.slot, left.room, left.choice)
           < std::tie(right.slot, right.room, right.choice);
  });
  Vector<sat::BoolVar> group;
  for (int begin = 0; begin < uses.size();) {
    int end = begin;
    group.clear();
    while (end < uses.size() && uses[end].slot == uses[begin].slot
           && uses[end].room == uses[begin].room) {
      group.push_back(choices[uses[end].choice]);
      ++end;
    }
    if (group.size() > 1) {
      model.AddAtMostOne(group.std_vector());
    }
    begin = end;
  }
}

auto read_assignment(
    const ProblemData& problem_data,
    const Vector<sat::BoolVar>& choices,
    const sat::CpSolverResponse& response,
    Status status
) -> RoomAssignment {
  CsrMatrix<int> rooms_by_section;
  rooms_by_section.reserve_num_rows(problem_data.num_sections);
  rooms_by_section.reserve_num_elements(problem_data.feasible_rooms_by_section.num_elements());
  int64_t unused_capacity = 0;
  int choice_index = 0;
  for (int section : problem_data.sections()) {
    for (int room : problem_data.feasible_rooms_by_section[section].values) {
      if (sat::SolutionBooleanValue(response, choices[choice_index])) {
        rooms_by_section.append(room);
        unused_capacity += problem_data.room_capacities[room];
      }
      ++choice_index;
    }
    unused_capacity -= problem_data.section_student_counts[section];
    rooms_by_section.finish_row();
  }
  return {
      .status = status,
      .rooms_by_section = std::move(rooms_by_section),
      .unused_capacity = unused_capacity
  };
}

auto empty_assignment(int num_sections, Status status) -> RoomAssignment {
  CsrMatrix<int> rooms_by_section;
  rooms_by_section.reserve_num_rows(num_sections);
  for (int i = 0; i < num_sections; ++i) {
    rooms_by_section.append_row({});
  }
  return {.status = status, .rooms_by_section = std::move(rooms_by_section), .unused_capacity = 0};
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

}  // namespace

auto assign_rooms(const ProblemData& problem_data, const Vector<int>& schedule, Params params)
    -> RoomAssignment {
  const auto started = Clock::now();
  validate_schedule(problem_data, schedule);

  sat::CpModelBuilder model;
  Vector<sat::BoolVar> choices;
  Vector<RoomUse> uses;
  const int num_choices = problem_data.feasible_rooms_by_section.num_elements();
  choices.reserve(num_choices);
  uses.reserve(num_choices);
  sat::LinearExpr room_count;
  sat::LinearExpr total_capacity;

  // Create variables only for eligible (section, room) pairs, in CSR row order.
  for (int section : problem_data.sections()) {
    sat::LinearExpr capacity;
    const int slot = schedule[problem_data.section_to_exam[section]];
    for (int room : problem_data.feasible_rooms_by_section[section].values) {
      const auto choice = model.NewBoolVar();
      uses.push_back({.slot = slot, .room = room, .choice = choices.size()});
      choices.push_back(choice);
      const int seats = problem_data.room_capacities[room];
      capacity += seats * choice;
      total_capacity += seats * choice;
      room_count += choice;
    }
    model.AddGreaterOrEqual(capacity, problem_data.section_student_counts[section]);
  }

  add_exclusive_room_constraints(model, choices, uses);

  const auto remaining_time = [&]() -> double {
    return params.max_time_seconds()
           - std::chrono::duration<double>(Clock::now() - started).count();
  };
  const auto solve = [&]() -> sat::CpSolverResponse {
    sat::SatParameters solver_params;
    solver_params.set_max_time_in_seconds(std::max(0.0, remaining_time()));
    solver_params.set_num_search_workers(params.num_workers());
    solver_params.set_random_seed(params.random_seed());
    sat::Model solver;
    solver.Add(sat::NewSatParameters(solver_params));
    return sat::SolveCpModel(model.Build(), &solver);
  };

  model.Minimize(room_count);
  if (remaining_time() <= 0.0) {
    return empty_assignment(problem_data.num_sections, Status::UNKNOWN);
  }
  auto best = solve();
  if (!has_solution(best)) {
    return empty_assignment(problem_data.num_sections, failure_status(best));
  }

  // Only optimize excess seats once the primary room count is proven minimal.
  // Two solves avoid large weighted coefficients and preserve exact lexicographic priority.
  Status status = Status::FEASIBLE;
  if (best.status() == sat::CpSolverStatus::OPTIMAL && remaining_time() > 0.0) {
    const int64_t minimum_rooms = sat::SolutionIntegerValue(best, room_count);
    model.AddEquality(room_count, minimum_rooms);
    model.Minimize(total_capacity);  // Enrollment is constant, so this minimizes unused seats.
    for (const auto choice : choices) {
      model.AddHint(choice, sat::SolutionBooleanValue(best, choice));
    }
    auto secondary = solve();
    if (has_solution(secondary)) {
      status =
          secondary.status() == sat::CpSolverStatus::OPTIMAL ? Status::OPTIMAL : Status::FEASIBLE;
      if (sat::SolutionIntegerValue(secondary, total_capacity)
          <= sat::SolutionIntegerValue(best, total_capacity)) {
        best = std::move(secondary);
      }
    } else if (secondary.status() == sat::CpSolverStatus::MODEL_INVALID) {
      return empty_assignment(problem_data.num_sections, Status::MODEL_INVALID);
    }
    // UNKNOWN in phase two preserves the complete phase-one incumbent as FEASIBLE.
  }

  return read_assignment(problem_data, choices, best, status);
}

}  // namespace cetp::rooms
