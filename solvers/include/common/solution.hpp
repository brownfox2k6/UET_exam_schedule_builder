#pragma once

#include "common/problem_data.hpp"
#include "utils/feasible_set.hpp"
#include "utils/matrix.hpp"

namespace common {

/**
 * @brief Mutable candidate timetable state shared by solver algorithms.
 * The schedule acts as a direct mapping: the vector index represents the Exam ID,
 * and the stored value represents the assigned Timeslot ID (-1 for unassigned).
 * The penalty tracks soft constraint violations (lower is better).
 */
struct Solution {
  // NOLINTBEGIN(*-non-private-*)

  // `conflicting_exams_count(i, j)` tells how many exams `k` that
  // have conflict with exam `i` and have been assigned to slot `j`.
  // If this value is > 0, slot `j` is strictly forbidden for exam `i`.
  utils::Matrix<int> conflicting_exams_count;

  // `assigned_slots[i]` tells the assigned slot for exam `i`.
  std::vector<int> assigned_slots;

  // Tracks available slot IDs for each exam.
  utils::FeasibleSet feasible_slots;

  // The total soft constraint penalty score of the current schedule (lower is better).
  double penalty;

  // NOLINTEND(*-non-private-*)

  /**
   * @brief Initializes an empty schedule and default tracking matrices.
   */
  Solution(const ProblemData& exams);

  /**
   * @brief Resets the schedule and conflict states to prepare for reuse.
   */
  void reset();

  /**
   * @brief Selects the next unscheduled exam using a constructive heuristic.
   */
  [[nodiscard]] auto get_next_exam(const ProblemData& problem_data) const -> int;

  /**
   * @brief Assigns an exam to a slot, accumulates penalty, and updates conflict/feasibility states
   * for neighbors.
   */
  void assign_exam(int exam, int slot, const utils::CsrMatrix<int>& conflicts_csrmatrix);

  /**
   * @brief Removes an exam from its current slot and restores the conflict/feasibility states for
   * neighbors.
   */
  void unassign_exam(int exam, const utils::CsrMatrix<int>& conflicts_csrmatrix);

};  // struct Solution

}  // namespace common