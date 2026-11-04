#pragma once

#include <span>

#include "common/problem_data.hpp"
#include "utils/available_set.hpp"

namespace common {

struct RoomAssignment {
  // NOLINTBEGIN(*-non-private-*)

  // `assigned_room_ids[offsets[i] + j]` is the ID of the j-th room assigned to section `i`.
  // `offsets` come from `ProblemData::feasible_rooms_by_section`.
  std::vector<int> assigned_room_ids;

  // `assigned_room_counts[i]` is the number of rooms currently assigned to section `i`.
  std::vector<int> assigned_room_counts;

  // `assigned_room_capacity[i]` is the sum of the full capacities of rooms assigned to section `i`.
  std::vector<int> assigned_room_capacity;

  // `available_rooms_by_slot[i]` contains rooms not yet assigned to any section in slot `i`.
  utils::AvailableSet available_rooms_by_slot;

  // NOLINTEND(*-non-private-*)

  RoomAssignment(const ProblemData& problem_data);

  /**
   * @brief Assigns a complete set of whole rooms to a section in a slot.
   * @pre Every room is statically feasible for the section and available in the slot.
   */
  void assign_section(const ProblemData& problem_data, int section, int slot,
                      std::span<const int> rooms);

  /**
   * @brief Removes all rooms assigned to a section and restores them to the slot's available set.
   */
  void unassign_section(const ProblemData& problem_data, int section, int slot);

  /**
   * @brief Clears room assignments and restores all rooms as available in every slot.
   */
  void reset();

 private:
  static auto build_available_rooms_by_slot(const ProblemData& problem_data)
      -> utils::CsrMatrix<int>;

};  // struct RoomAssignment

}  // namespace common
