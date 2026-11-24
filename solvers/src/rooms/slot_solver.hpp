#pragma once

#include <cstdint>

#include "problem_data.hpp"
#include "rooms/room_assignment.hpp"
#include "utils/jagged_array.hpp"
#include "utils/span.hpp"

namespace cetp::rooms {

/** Result of optimizing room assignment for a single independent time slot. */
struct SlotSolution {
  RoomAssignmentStatus status = RoomAssignmentStatus::OPTIMAL;
  JaggedArray<int> rooms_by_local_section{};  // NOLINT(readability-redundant-member-init)
  int64_t extra_locations = 0;
  int64_t unused_capacity = 0;
};

/**
 * Solves room allocation for one non-empty time slot using a single weighted CP-SAT model that
 * strictly prioritizes room count, exam campus consolidation, then unused seat capacity.
 */
[[nodiscard]] auto solve_slot(
    const ProblemData& problem,
    Span<const int> slot_exams,
    double time_limit,
    int solver_seed
) -> SlotSolution;

}  // namespace cetp::rooms
