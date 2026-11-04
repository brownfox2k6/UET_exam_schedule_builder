#pragma once

#include "common/problem_data.hpp"
#include "common/room_assignment.hpp"
#include "common/slot_assignment.hpp"

namespace common {

/**
 * @brief Mutable candidate timetable state shared by solver algorithms.
 * The penalty tracks soft constraint violations (lower is better).
 */
struct Solution {
  // NOLINTBEGIN(*-non-private-*)

  SlotAssignment slot_assignment;
  RoomAssignment room_assignment;

  // The total soft constraint penalty score of the current schedule (lower is better).
  double penalty{0.0};

  // NOLINTEND(*-non-private-*)

  /**
   * @brief Initializes an empty schedule and default tracking matrices.
   */
  Solution(const ProblemData& problem_data);

  /**
   * @brief Resets the schedule and conflict states to prepare for reuse.
   */
  void reset();
};  // struct Solution

}  // namespace common
