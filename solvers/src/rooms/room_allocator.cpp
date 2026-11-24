#include "rooms/room_allocator.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <utility>

#include "problem_data.hpp"
#include "rooms/params.hpp"
#include "rooms/room_assignment.hpp"
#include "rooms/slot_solver.hpp"
#include "timetable.hpp"
#include "utils/csr_matrix.hpp"
#include "utils/span.hpp"
#include "utils/vector.hpp"

namespace cetp::rooms {
namespace {

using Status = RoomAssignmentStatus;

auto elapsed_seconds(SteadyClock::time_point start_time) -> double {
  return std::chrono::duration<double>(SteadyClock::now() - start_time).count();
}

// Groups exams by their assigned time slot in a flat CSR matrix so every slot can be solved
// independently without nested heap allocations.
auto group_exams_by_slot(const ProblemData& problem, const Vector<int>& schedule)
    -> CsrMatrix<int> {
  Vector<int> offsets(problem.num_slots + 1, 0);
  for (int exam : problem.exams()) {
    ++offsets[schedule[exam] + 1];
  }
  for (int slot : problem.slots()) {
    offsets[slot + 1] += offsets[slot];
  }
  Vector<int> values(problem.num_exams);
  Vector<int> cursor = offsets;
  for (int exam : problem.exams()) {
    values[cursor[schedule[exam]]++] = exam;
  }
  return {std::move(values), std::move(offsets)};
}

auto collect_active_slots(const ProblemData& problem, const CsrMatrix<int>& exams_by_slot)
    -> Vector<int> {
  Vector<int> active_slots;
  active_slots.reserve(problem.num_slots);
  for (int slot : problem.slots()) {
    if (!exams_by_slot[slot].values.empty()) {
      active_slots.push_back(slot);
    }
  }
  return active_slots;
}

// Combines per-slot results into the final global RoomAssignment, enforcing all-or-nothing when
// any slot fails while preserving per-slot diagnostic statuses.
auto assemble_room_assignment(
    const ProblemData& problem,
    const CsrMatrix<int>& exams_by_slot,
    const Vector<int>& active_slots,
    const Vector<SlotSolution>& slot_solutions
) -> RoomAssignment {
  Vector<Status> slot_statuses(problem.num_slots, Status::OPTIMAL);
  Status overall_status = Status::OPTIMAL;
  for (int slot : problem.slots()) {
    slot_statuses[slot] = slot_solutions[slot].status;
    overall_status = worse_status(overall_status, slot_statuses[slot]);
  }

  if (overall_status != Status::OPTIMAL && overall_status != Status::FEASIBLE) {
    return RoomAssignment::empty(problem.num_sections, overall_status, std::move(slot_statuses));
  }

  // Scatter per-slot CSR views back into global section index order [0, num_sections).
  Vector<Span<const int>> rooms_by_section_spans(problem.num_sections);
  int total_rooms = 0;
  int64_t total_extra_locations = 0;
  int64_t total_unused_capacity = 0;

  for (int slot : active_slots) {
    const SlotSolution& solution = slot_solutions[slot];
    total_extra_locations += solution.extra_locations;
    total_unused_capacity += solution.unused_capacity;
    int local_section_index = 0;
    for (int exam : exams_by_slot[slot].values) {
      for (int section : problem.sections_of_exam(exam)) {
        const Span<const int> assigned_rooms =
            solution.rooms_by_local_section[local_section_index++].values;
        rooms_by_section_spans[section] = assigned_rooms;
        total_rooms += assigned_rooms.size();
      }
    }
  }

  CsrMatrix<int> rooms_by_section;
  rooms_by_section.reserve_num_rows(problem.num_sections);
  rooms_by_section.reserve_num_elements(total_rooms);
  for (int section : problem.sections()) {
    rooms_by_section.append_row(rooms_by_section_spans[section]);
  }

  return {
      .status = overall_status,
      .rooms_by_section = std::move(rooms_by_section),
      .slot_statuses = std::move(slot_statuses),
      .extra_locations = total_extra_locations,
      .unused_capacity = total_unused_capacity
  };
}

}  // namespace

auto assign_rooms(const ProblemData& problem_data, const Vector<int>& schedule, Params params)
    -> RoomAssignment {
  const auto global_started = SteadyClock::now();
  validate_schedule(problem_data, schedule);

  // Decompose by time slot: exams in different slots never share rooms or constraints, so each
  // non-empty slot can be solved as an independent CP-SAT model.
  const CsrMatrix<int> exams_by_slot = group_exams_by_slot(problem_data, schedule);
  const Vector<int> active_slots = collect_active_slots(problem_data, exams_by_slot);

  Vector<SlotSolution> slot_solutions(problem_data.num_slots);
  std::atomic<bool> any_slot_failed{false};

  const int active_slot_count = active_slots.size();
  const int effective_workers = std::min(params.num_workers(), std::max(1, active_slot_count));

  if (effective_workers == 1) {
    // Sequential mode: divide remaining wall-clock time across remaining slots so easy slots
    // finishing in milliseconds leave nearly the entire budget for harder slots.
    for (int index = 0; index < active_slot_count; ++index) {
      const int slot = active_slots[index];
      const double remaining = params.max_time_seconds() - elapsed_seconds(global_started);
      const double per_slot_limit = remaining / double(active_slot_count - index);
      slot_solutions[slot] = solve_single_slot(
          {.problem = problem_data,
           .slot_exams = exams_by_slot[slot].values,
           .slot = slot,
           .slot_time_limit = per_slot_limit,
           .global_started = global_started,
           .global_time_limit = params.max_time_seconds(),
           .base_seed = params.random_seed(),
           .any_slot_failed = any_slot_failed}
      );
    }
  } else {
    // Parallel mode: solve independent slots concurrently across OpenMP threads.
    // Scaling the per-slot cap by `effective_workers / active_slot_count` lets concurrent batches
    // use the full wall-clock budget while `global_time_limit` bounds total elapsed time.
    const double per_slot_limit =
        params.max_time_seconds() * double(effective_workers) / double(active_slot_count);
#pragma omp parallel for schedule(dynamic) num_threads(effective_workers)
    for (int index = 0; index < active_slot_count; ++index) {
      const int slot = active_slots[index];
      slot_solutions[slot] = solve_single_slot(
          {.problem = problem_data,
           .slot_exams = exams_by_slot[slot].values,
           .slot = slot,
           .slot_time_limit = per_slot_limit,
           .global_started = global_started,
           .global_time_limit = params.max_time_seconds(),
           .base_seed = params.random_seed(),
           .any_slot_failed = any_slot_failed}
      );
    }
  }

  return assemble_room_assignment(problem_data, exams_by_slot, active_slots, slot_solutions);
}

}  // namespace cetp::rooms
