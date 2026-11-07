#pragma once

#include "common/problem_data.hpp"
#include "utils/available_set.hpp"
#include "utils/matrix.hpp"

namespace common {

struct SlotAssignment {
  // NOLINTBEGIN(*-non-private-*)

  // `conflicting_exams_count(i, j)` tells how many exams `k` that
  // have conflict with exam `i` and have been assigned to slot `j`.
  // If this value is > 0, slot `j` is strictly forbidden for exam `i`.
  Matrix<int> conflicting_exams_count;

  // `assigned_slots[i]` tells the assigned slot for exam `i`.
  Vector<int> assigned_slots;

  // `available_slots_by_exam[i]` tells the currently available slots for exam `i`.
  AvailableSet available_slots_by_exam;

  // NOLINTEND(*-non-private-*)

  SlotAssignment(const ProblemData& problem_data);

  /**
   * @brief Selects the next unscheduled exam using a constructive heuristic.
   */
  [[nodiscard]] auto get_next_exam(const ProblemData& problem_data) const -> int;

  /**
   * @brief Assigns an exam to a slot and updates conflict/availability states for neighbors.
   */
  void assign_exam(int exam, int slot, const CsrMatrix<int>& conflicts_csrmatrix);

  /**
   * @brief Removes an exam from its current slot,
   * and restores the conflict/availability states for neighbors.
   */
  void unassign_exam(int exam, const CsrMatrix<int>& conflicts_csrmatrix);

  /**
   * @brief Resets the schedule and conflict states to prepare for reuse.
   */
  void reset();

};  // struct SlotAssignment

}  // namespace common
