#pragma once

#include <cassert>
#include <cstddef>
#include <vector>

#include "utils/feasible_set.hpp"
#include "utils/matrix.hpp"

namespace common {

/**
* @brief Represents an individual ant constructing an exam timetable.
* The schedule acts as a direct mapping: the vector index represents the Exam ID, 
* and the stored value represents the assigned Timeslot ID (-1 for unassigned).
* The fitness tracks soft constraint violations (lower is better).
*/
struct Solution {
private:
  int num_exams;
  int num_slots;

public:
  // `conflicting_exams_count(i, j)` tells how many exams `k` that 
  // have conflict with exam `i` and have been assigned to slot `j`.
  // If this value is > 0, slot `j` is strictly forbidden for exam `i`.
  utils::Matrix<int> conflicting_exams_count;

  // `schedule[i]` tells the assigned slot for exam `i`
  std::vector<int> schedule;

  // Tracks available slot IDs for each exam.
  utils::FeasibleSet<int> feasible_slots;

  // The total soft constraint penalty score of the current schedule (lower is better).
  double fitness;

  /**
   * @brief Initializes an ant with empty schedule and default tracking matrices. 
   */
  Solution(int n_exams, int n_slots);

  /**
   * @brief Overloads the less-than operator for Ant comparison.
   * * Used by standard algorithms like std::min_element or std::sort.
   * An ant is considered "less than" another if it has a lower penalty score (fitness).
   * * @param other The ant to compare with.
   * @return true if this ant's fitness is strictly less than the other's.
   */
  bool operator<(const Solution& other) const;

  /**
   * @brief Resets the ant's memory, schedule, and conflict states to prepare for a new iteration. 
   */
  void reset();

  /**
   * Heuristic: Exam scheduling order
   * Find the next exam to schedule based on the Saturation Degree heuristic.
   * Prioritize the exam with the fewest feasible slots (in other words, the most forbidden slots).
   * If there is a tie, select the exam with the highest total conflict degree (against all other exams).
   */
  int get_next_exam(const std::vector<int>& total_student_conflict) const;

  /**
   * @brief Assigns an exam to a slot, accumulates penalty, and updates conflict/feasibility states for neighbors.
   */
  void assign_exam(
    int exam,
    int slot,
    const utils::CsrMatrix<int>& student_conflicts
  );

  /**
   * @brief Removes an exam from its current slot and restores the conflict/feasibility states for neighbors.
   */
  void unassign_exam(
    int exam,
    const utils::CsrMatrix<int>& student_conflicts
  );
};

} // namespace common