#include "rooms/room_allocator.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <utility>

#include "problem_data.hpp"
#include "rooms/params.hpp"
#include "rooms/room_assignment.hpp"
#include "rooms/slot_solver.hpp"
#include "timetable.hpp"
#include "utils/jagged_array.hpp"
#include "utils/span.hpp"
#include "utils/vector.hpp"

namespace cetp::rooms {
namespace {

// Non-negative 31-bit mask for OR-Tools `SatParameters::random_seed` (int32).
constexpr uint64_t SEED_MASK = 0x7fffffffULL;

// Groups exams by assigned time slot via counting sort into a flat JaggedArray (one row per slot).
[[nodiscard]] auto group_exams_by_slot(const ProblemData& problem, const Vector<int>& schedule)
    -> JaggedArray<int> {
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

// Merges independent per-slot solutions back into global section order [0, num_sections).
// Uses all-or-nothing semantics: if any slot fails, returns an empty RoomAssignment.
[[nodiscard]] auto assemble_result(
    const ProblemData& problem,
    const JaggedArray<int>& exams_by_slot,
    const Vector<SlotSolution>& slot_solutions
) -> RoomAssignment {
  auto overall_status = RoomAssignmentStatus::OPTIMAL;
  for (int slot : problem.slots()) {
    overall_status = std::max(overall_status, slot_solutions[slot].status);
  }
  if (!is_found(overall_status)) {
    return {.status = overall_status};
  }

  Vector<Span<const int>> rooms_by_section_spans(problem.num_sections);
  int total_rooms = 0;
  int64_t total_extra_locations = 0;
  int64_t total_unused_capacity = 0;

  for (int slot : problem.slots()) {
    const SlotSolution& solution = slot_solutions[slot];
    total_rooms += solution.rooms_by_local_section.num_elements();
    total_extra_locations += solution.extra_locations;
    total_unused_capacity += solution.unused_capacity;
    int local_section = 0;
    for (int exam : exams_by_slot[slot]) {
      for (int section : problem.sections_of_exam(exam)) {
        rooms_by_section_spans[section] = solution.rooms_by_local_section[local_section++];
      }
    }
  }

  JaggedArray<int> rooms_by_section;
  rooms_by_section.reserve_num_rows(problem.num_sections);
  rooms_by_section.reserve_num_elements(total_rooms);
  for (int section : problem.sections()) {
    rooms_by_section.append_row(rooms_by_section_spans[section]);
  }

  return {
      .status = overall_status,
      .rooms_by_section = std::move(rooms_by_section),
      .extra_locations = total_extra_locations,
      .unused_capacity = total_unused_capacity
  };
}

}  // namespace

auto assign_rooms(
    const ProblemData& problem,
    const Vector<int>& schedule,
    Params params,
    uint64_t base_seed
) -> RoomAssignment {
  validate_schedule(problem, schedule);

  // Exams in different time slots never share rooms, so each non-empty slot is solved separately.
  const JaggedArray<int> exams_by_slot = group_exams_by_slot(problem, schedule);
  const int workers = std::min(params.num_workers(), problem.num_slots);

  Vector<SlotSolution> slot_solutions(problem.num_slots);
  // Short-circuit only on INFEASIBLE (the most severe status) so mixed UNKNOWN/INFEASIBLE
  // schedules stay deterministic across worker counts.
  std::atomic<bool> abort_remaining{false};

#pragma omp parallel for schedule(dynamic) num_threads(workers)
  for (int slot = 0; slot < problem.num_slots; ++slot) {
    if (exams_by_slot[slot].empty() || abort_remaining.load(std::memory_order_relaxed)) {
      continue;
    }
    const int slot_seed = int((base_seed + uint64_t(slot)) & SEED_MASK);
    slot_solutions[slot] =
        solve_slot(problem, exams_by_slot[slot], params.slot_time_limit(), slot_seed);
    if (slot_solutions[slot].status == RoomAssignmentStatus::INFEASIBLE) {
      abort_remaining.store(true, std::memory_order_relaxed);
    }
  }

  return assemble_result(problem, exams_by_slot, slot_solutions);
}

}  // namespace cetp::rooms
