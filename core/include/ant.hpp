#pragma once

#include <cstddef>
#include <vector>
#include <cassert>
#include <cmath>
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
  Ant(size_t n_exams, size_t n_slots)
    : num_exams(n_exams),
      num_slots(n_slots),
      schedule(num_exams, -1),
      feasible_slots(num_exams, num_slots),
      feasible_slots_count(num_exams, num_slots),
      fitness(0.0),
      conflicting_exams_count(num_exams, num_slots, 0),
      forbidden_slots_count(num_exams, 0) 
  {
    reset_feasible_slots();
  }

  /**
    * WARNING: This is a PARTIAL assignment.
    * It only copies `schedule` and `fitness` for performance when saving the global_best.
    * Do NOT use this to duplicate an Ant for local search or further modifications!
    */
  Ant& operator=(const Ant& other) {
    if (this != &other) {
      this->schedule = other.schedule;
      this->fitness = other.fitness;
    }
    return *this;
  }

  /**
   * @brief Re-initializes the feasible slots tracking matrix to its initial state (all slots available). 
   */
  void reset_feasible_slots() {
    for (size_t exam = 0; exam < num_exams; ++exam) {
      for (size_t slot = 0; slot < num_slots; ++slot) {
        feasible_slots(exam, slot) = slot;
      }
    }
  }

  /**
   * @brief Resets the ant's memory, schedule, and conflict states to prepare for a new iteration. 
   */
  void reset() {
    std::fill(schedule.begin(), schedule.end(), -1);
    reset_feasible_slots();
    std::fill(feasible_slots_count.begin(), feasible_slots_count.end(), num_slots);
    fitness = 0.0;
    conflicting_exams_count.fill(0);
    std::fill(forbidden_slots_count.begin(), forbidden_slots_count.end(), 0);
  }

  /**
   * Heuristic: Exam scheduling order
   * Find the next exam to schedule based on the Saturation Degree heuristic.
   * Prioritize the exam with the fewest feasible slots (in other words, the most forbidden slots).
   * If there is a tie, select the exam with the highest total conflict degree (against all other exams).
   */
  int get_next_exam(const std::vector<int>& total_student_conflict) const {
    int best_exam = -1;
    int max_degree = -1;
    int max_conflict = -1;
    for (size_t exam = 0; exam < num_exams; ++exam) {
      if (schedule[exam] != -1) {
        continue;
      }
      int degree = forbidden_slots_count[exam];
      int conflict = total_student_conflict[exam];
      if (std::tie(degree, conflict) > std::tie(max_degree, max_conflict)) {
        max_degree = degree;
        max_conflict = conflict;
        best_exam = exam;
      }
    }
    return best_exam;
  }

  /**
   * @brief Assigns an exam to a slot, accumulates penalty, and updates conflict/feasibility states for neighbors.
   */
  void assign_exam(
    int exam,
    int slot,
    const common::CsrMatrix<int>& conflict_exams,
    double penalty = 0.0
  ) {
    schedule[exam] = slot;
    fitness += penalty;
    auto [first, last] = conflict_exams.row_range(exam);
    for (auto it = first; it != last; ++it) {
      int neighbor_exam = *it;
      if (conflicting_exams_count(neighbor_exam, slot) == 0) {
        ++forbidden_slots_count[neighbor_exam];
        int target = -1;
        for (size_t j = 0; j < feasible_slots_count[neighbor_exam]; ++j) {
          if (feasible_slots(neighbor_exam, j) == slot) {
            target = j;
            break;
          }
        }
        if (target != -1) {
          feasible_slots(neighbor_exam, target)
            = feasible_slots(neighbor_exam, --feasible_slots_count[neighbor_exam]);
          ;
        }
      }
      ++conflicting_exams_count(neighbor_exam, slot);
    }
  }

  /**
   * @brief Removes an exam from its current slot and restores the conflict/feasibility states for neighbors.
   */
  void unassign_exam(
    int exam,
    const common::CsrMatrix<int>& conflict_exams
  ) {
    int old_slot = schedule[exam];
    schedule[exam] = -1;
    auto [first, last] = conflict_exams.row_range(exam);
    for (auto it = first; it != last; ++it) {
      int neighbor_exam = *it;
      if (--conflicting_exams_count(neighbor_exam, old_slot) == 0) {
        --forbidden_slots_count[neighbor_exam];
        feasible_slots(neighbor_exam, feasible_slots_count[neighbor_exam]++) = old_slot;
      }
    }
  }

  /**
   * @brief Calculates the change in fitness (soft penalty) if an exam is placed in or moved to a new slot.
   */
  double calculate_delta_penalty(
    int exam,
    int new_slot,
    const common::Matrix<int>& student_conflict,
    const common::CsrMatrix<int>& conflict_exams,
    const std::vector<double>& absolute_day_slots,
    const int ignore_exam = -1
  ) const {
    double delta = 0.0;
    double d_old = schedule[exam] != -1 ? absolute_day_slots[schedule[exam]] : -1.0;
    double d_new = absolute_day_slots[new_slot];
    auto [first, last] = conflict_exams.row_range(exam);
    for (auto it = first; it != last; ++it) {
      int e = *it;
      int weight = student_conflict(exam, e);
      if (weight == 0 || e == ignore_exam || e == exam || schedule[e] == -1) {
        continue;
      }
      double d_cur = absolute_day_slots[schedule[e]];
      double penalty_old = schedule[exam] != -1 ? weight * std::exp2(-std::fabs(d_old - d_cur)) : 0.0;
      double penalty_new = weight * std::exp2(-std::fabs(d_new - d_cur));
      delta += penalty_new - penalty_old;
    }
    return delta;
  }
};

} // namespace aco