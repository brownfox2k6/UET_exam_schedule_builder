#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

#include "problem_data.hpp"
#include "rooms/room_assignment.hpp"
#include "utils/jagged_array.hpp"
#include "utils/span.hpp"

namespace cetp::rooms {

using SteadyClock = std::chrono::steady_clock;

/** Result of optimizing room assignment for a single independent time slot. */
struct SlotSolution {
  RoomAssignmentStatus status = RoomAssignmentStatus::OPTIMAL;
  // Local section index -> assigned room IDs (in ascending room ID order).
  JaggedArray<int> rooms_by_local_section{};  // NOLINT(readability-redundant-member-init)
  int64_t extra_locations = 0;
  int64_t unused_capacity = 0;
};

/** Inputs and shared deadline/failure state for solving one independent time slot. */
struct SlotSolveRequest {
  // NOLINTBEGIN(cppcoreguidelines-avoid-const-or-ref-data-members)
  const ProblemData& problem;
  Span<const int> slot_exams;
  int slot = 0;
  double slot_time_limit = 0.0;
  SteadyClock::time_point global_started;
  double global_time_limit = 0.0;
  int base_seed = 0;
  std::atomic<bool>& any_slot_failed;
  // NOLINTEND(cppcoreguidelines-avoid-const-or-ref-data-members)
};

/**
 * Solves one non-empty time slot using 3-phase lexicographic CP-SAT optimization:
 *   1. Minimize `room_count`,
 *   2. Keeping `room_count` optimal, minimize `extra_locations`,
 *   3. Keeping both optimal, minimize `total_capacity` (unused seats).
 */
[[nodiscard]] auto solve_single_slot(const SlotSolveRequest& request) -> SlotSolution;

}  // namespace cetp::rooms
