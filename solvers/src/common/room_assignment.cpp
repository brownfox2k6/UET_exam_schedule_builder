#include "common/room_assignment.hpp"

#include <algorithm>
#include <cstddef>

#include "utils/matrix.hpp"

namespace common {

RoomAssignment::RoomAssignment(const ProblemData& problem_data)
    : assigned_room_ids(size_t(problem_data.feasible_rooms_by_section.num_elements()), -1),
      assigned_room_counts(size_t(problem_data.num_sections), 0),
      assigned_room_capacity(size_t(problem_data.num_sections), 0),
      available_rooms_by_slot(build_available_rooms_by_slot(problem_data), problem_data.num_rooms) {
}

void RoomAssignment::reset() {
  std::ranges::fill(assigned_room_ids, -1);
  std::ranges::fill(assigned_room_counts, 0);
  std::ranges::fill(assigned_room_capacity, 0);
  available_rooms_by_slot.reset();
}

auto RoomAssignment::build_available_rooms_by_slot(const ProblemData& problem_data)
    -> utils::CsrMatrix<int> {
  const int num_slots = problem_data.num_slots;
  const int num_rooms = problem_data.num_rooms;
  utils::Matrix<int> room_options(num_slots, num_rooms, -1);
  for (int slot = 0; slot < num_slots; ++slot) {
    for (int room = 0; room < num_rooms; ++room) {
      room_options[slot, room] = room;
    }
  }
  const int expected_size = num_slots * num_rooms;
  return {room_options, expected_size, false, -1};
}

}  // namespace common
