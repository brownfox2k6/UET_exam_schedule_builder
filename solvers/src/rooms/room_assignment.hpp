#pragma once

#include <cstdint>

#include "utils/jagged_array.hpp"
#include "utils/span.hpp"
#include "utils/vector.hpp"

namespace cetp::rooms {

enum class RoomAssignmentStatus : std::uint8_t {
  OPTIMAL,
  FEASIBLE,
  INFEASIBLE,
  UNKNOWN,
  MODEL_INVALID
};

/**
 * Room allocation for a fixed schedule.
 * Room IDs are stored per section in a flat `JaggedArray` (including ID zero). `slot_statuses`
 * records the per-slot solver outcome even when overall assignment is cleared on failure.
 */
struct RoomAssignment {
  // NOLINTBEGIN(*-non-private-*)
  RoomAssignmentStatus status{};
  JaggedArray<int> rooms_by_section;
  Vector<RoomAssignmentStatus> slot_statuses;
  int64_t extra_locations{};
  int64_t unused_capacity{};
  // NOLINTEND(*-non-private-*)

  [[nodiscard]] auto found() const noexcept -> bool {
    return status == RoomAssignmentStatus::OPTIMAL || status == RoomAssignmentStatus::FEASIBLE;
  }

  [[nodiscard]] auto room_count() const noexcept -> int { return rooms_by_section.num_elements(); }

  [[nodiscard]] auto rooms_for_section(int section) const -> Span<const int>;

  [[nodiscard]] static auto empty(
      int num_sections,
      RoomAssignmentStatus overall_status,
      Vector<RoomAssignmentStatus> slot_statuses
  ) -> RoomAssignment;
};  // struct RoomAssignment

/** Returns whichever status is more severe when aggregating independent per-slot outcomes. */
[[nodiscard]] auto worse_status(
    RoomAssignmentStatus first,
    RoomAssignmentStatus second
) noexcept -> RoomAssignmentStatus;

}  // namespace cetp::rooms
