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

  // Number of conflicting exams currently assigned to each (exam, slot) pair.
  // Any count > 0 strictly forbids assigning that exam to that slot.
  Matrix<int> conflicting_exams_count;

  // Assigned slot index for each exam, or -1 if unscheduled.
  Vector<int> assigned_slots;

  // Currently available (feasible and conflict-free) slots for each exam.
  AvailableSet available_slots_by_exam;

  // Incremental soft-constraint penalty maintained by the search algorithm.
  double penalty = 0.0;

  // NOLINTEND(*-non-private-*)

  explicit SlotAssignment(const ProblemData& problem);

  /** Assigns an exam to a slot and updates conflict and availability states for neighbors. */
  void assign_exam(int exam, int slot, const CsrMatrix<int>& conflicts_csr);

  /** Removes an exam from its current slot and restores neighbor conflict and availability. */
  void unassign_exam(int exam, const CsrMatrix<int>& conflicts_csr);

  /** Resets the schedule, conflict counts, and penalty for reuse. */
  void reset();
};  // struct SlotAssignment

}  // namespace cetp::slots
