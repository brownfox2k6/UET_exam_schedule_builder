#pragma once

#include "problem_data.hpp"
#include "utils/available_set.hpp"
#include "utils/csr_matrix.hpp"
#include "utils/matrix.hpp"
#include "utils/vector.hpp"

namespace cetp::slots {

/** Mutable candidate state used by the slot-search algorithms. */
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

  // The search algorithm applies the score change when it accepts a move.
  double penalty{0.0};

  // NOLINTEND(*-non-private-*)

  explicit SlotAssignment(const ProblemData& problem_data);

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

}  // namespace cetp::slots
