#include "rooms/room_assignment.hpp"

#include <stdexcept>

#include "utils/span.hpp"

namespace cetp::rooms {

auto RoomAssignment::rooms_for_section(int section) const -> Span<const int> {
  if (section < 0 || section >= rooms_by_section.num_rows()) {
    throw std::out_of_range("Section index out of range");
  }
  return rooms_by_section[section].values;
}

}  // namespace cetp::rooms
