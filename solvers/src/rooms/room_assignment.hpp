#pragma once

#include <cstdint>
#include <stdexcept>

#include "utils/jagged_array.hpp"
#include "utils/span.hpp"

namespace cetp::rooms {

// Ordered from best outcome to most severe failure so status aggregation is simply `std::max`.
enum class RoomAssignmentStatus : std::uint8_t { OPTIMAL, FEASIBLE, UNKNOWN, INFEASIBLE };

/** Returns true when `status` represents a valid (optimal or feasible) room allocation. */
[[nodiscard]] constexpr auto is_found(RoomAssignmentStatus status) noexcept -> bool {
  return status == RoomAssignmentStatus::OPTIMAL || status == RoomAssignmentStatus::FEASIBLE;
}

/**
 * Room allocation for a fixed schedule.
 * Room IDs are stored per section in a flat `JaggedArray` (including ID zero).
 */
struct RoomAssignment {
  // NOLINTBEGIN(*-non-private-*)
  RoomAssignmentStatus status{};
  JaggedArray<int> rooms_by_section{};  // NOLINT(readability-redundant-member-init)
  int64_t extra_locations{};
  int64_t unused_capacity{};
  // NOLINTEND(*-non-private-*)

  [[nodiscard]] auto found() const noexcept -> bool { return is_found(status); }

  [[nodiscard]] auto room_count() const noexcept -> int { return rooms_by_section.num_elements(); }

  [[nodiscard]] auto rooms_for_section(int section) const -> Span<const int> {
    if (section < 0 || section >= rooms_by_section.num_rows()) {
      throw std::out_of_range("Section index out of range");
    }
    return rooms_by_section[section];
  }
};  // struct RoomAssignment

}  // namespace cetp::rooms
