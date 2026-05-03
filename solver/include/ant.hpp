#pragma once

#include <cstddef>
#include <vector>
#include <cassert>
#include "matrix.hpp"
#include "csr_matrix.hpp"

namespace aco {

/**
* @brief Represents an individual ant constructing an exam timetable.
* The schedule acts as a direct mapping: the vector index represents the Exam ID, 
* and the stored value represents the assigned Timeslot ID (-1 for unassigned).
* The fitness tracks soft constraint violations (lower is better).
*/
struct Ant {
private:
  const size_t num_exams;
  const size_t num_slots;

public:
  // `conflicting_exams_count(i, j)` tells how many exams `k` that 
  // have conflict with exam `i` and have been assigned to slot `j`.
  // If this value is > 0, slot `j` is strictly forbidden for exam `i`.
  common::Matrix<int> conflicting_exams_count;

  // `forbidden_slots_count[i]` tells how many slots that exam `i` cannot be assigned to. 
  // That is, `forbidden_slots_count[i] = count(conflicting_exams_count(i, j) > 0)`
  std::vector<int> forbidden_slots_count;

  // `feasible_slots_count[i]` tells how many slots that exam `i` can be assigned to. 
  // That is, `feasible_slots_count[i] = count(conflicting_exams_count(i, j) == 0)`
  std::vector<int> feasible_slots_count;

  // `schedule[i]` tells the assigned slot for exam `i`
  std::vector<int> schedule;

  // A dense matrix tracking available slot IDs for each exam. 
  // Used alongside `feasible_slots_count` to allow O(1) random selection of a valid slot.
  common::Matrix<int> feasible_slots;

  // The total soft constraint penalty score of the current schedule (lower is better).
  double fitness;

  /**
   * @brief Initializes an ant with empty schedule and default tracking matrices. 
   */
  Ant(size_t n_exams, size_t n_slots);

  /**
   * WARNING: This is a PARTIAL assignment.
   * It only copies `schedule` and `fitness` for performance when saving the global_best.
   * Do NOT use this to duplicate an Ant for local search or further modifications!
   */
  Ant& operator=(const Ant& other);

  /**
   * @brief Overloads the less-than operator for Ant comparison.
   * * Used by standard algorithms like std::min_element or std::sort.
   * An ant is considered "less than" another if it has a lower penalty score (fitness).
   * * @param other The ant to compare with.
   * @return true if this ant's fitness is strictly less than the other's.
   */
  bool operator<(const Ant& other) const;

  /**
   * @brief Re-initializes the feasible slots tracking matrix to its initial state (all slots available). 
   */
  void reset_feasible_slots();

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
    const common::CsrMatrix<int>& student_conflicts
  );

  /**
   * @brief Removes an exam from its current slot and restores the conflict/feasibility states for neighbors.
   */
  void unassign_exam(
    int exam,
    const common::CsrMatrix<int>& student_conflicts
  );

  /**
   * @brief Calculates the change in fitness (soft penalty) if an exam is placed in or moved to a new slot.
   */
  double calculate_delta_penalty(
    int exam,
    int new_slot,
    const common::CsrMatrix<int>& student_conflicts,
    const common::Matrix<double>& proximity_penalties,
    int ignore_exam = -1
  ) const;
};

} // namespace aco