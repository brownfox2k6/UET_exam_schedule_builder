#include "rooms/room_assignment.hpp"

#include <cstdint>
#include <stdexcept>
#include <utility>

#include "utils/csr_matrix.hpp"
#include "utils/span.hpp"
#include "utils/vector.hpp"

namespace cetp::rooms {
namespace {

// Ordering from best outcome to worst outcome so status aggregation avoids magic numbers.
enum class StatusSeverity : std::uint8_t {
  OPTIMAL,
  FEASIBLE,
  UNKNOWN,
  INFEASIBLE,
  MODEL_INVALID
};

auto to_severity(RoomAssignmentStatus status) noexcept -> StatusSeverity {
  switch (status) {
  case RoomAssignmentStatus::OPTIMAL:
    return StatusSeverity::OPTIMAL;
  case RoomAssignmentStatus::FEASIBLE:
    return StatusSeverity::FEASIBLE;
  case RoomAssignmentStatus::UNKNOWN:
    return StatusSeverity::UNKNOWN;
  case RoomAssignmentStatus::INFEASIBLE:
    return StatusSeverity::INFEASIBLE;
  case RoomAssignmentStatus::MODEL_INVALID:
    return StatusSeverity::MODEL_INVALID;
  }
  return StatusSeverity::MODEL_INVALID;
}

}  // namespace

auto RoomAssignment::rooms_for_section(int section) const -> Span<const int> {
  if (section < 0 || section >= rooms_by_section.num_rows()) {
    throw std::out_of_range("Section index out of range");
  }
  return rooms_by_section[section].values;
}

auto RoomAssignment::empty(
    int num_sections,
    RoomAssignmentStatus overall_status,
    Vector<RoomAssignmentStatus> slot_statuses
) -> RoomAssignment {
  CsrMatrix<int> rooms_by_section;
  rooms_by_section.reserve_num_rows(num_sections);
  for (int section = 0; section < num_sections; ++section) {
    rooms_by_section.append_row({});
  }
  return {
      .status = overall_status,
      .rooms_by_section = std::move(rooms_by_section),
      .slot_statuses = std::move(slot_statuses),
      .extra_locations = 0,
      .unused_capacity = 0
  };
}

auto worse_status(RoomAssignmentStatus first, RoomAssignmentStatus second) noexcept
    -> RoomAssignmentStatus {
  return to_severity(second) > to_severity(first) ? second : first;
}

}  // namespace cetp::rooms
