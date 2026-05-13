#pragma once

#include "hyperparameters.hpp"
#include "matrix.hpp"

namespace common {

struct Evaluator {

  const common::Hyperparams::Evaluation hyperparams;
  const int num_exams;
  const int num_slots;
  
  /**
   * @brief Precalculated time-gap penalties for O(1) lookup.
   * * `proximity_penalties(i, j)` tells the closeness between slot `i` and `j`.
   */
  const common::Matrix<double> proximity_penalties;
  
  /**
    * @brief Matrix reprsentation of `student_conflicts`.
    * * `student_conflict(i, j)` tells the number of students taking both exams `i` and `j`
    */
  const common::Matrix<int> student_conflicts_matrix;

  /**
   * @brief Compressed representation of the exam conflict graph.
   * * Each row `i` contains a list of `CsrElement` objects, where:
   * - `index`: The ID of exam `j` that has at least one student in common with exam `i`.
   * - `value`: The exact number of students shared between exam `i` and `j`.
   */
  const common::CsrMatrix<int> student_conflicts;

  /**
   * @brief The total weighted conflict degree for each exam.
   * * For each exam `i`, this stores the sum of weights (students) across all its 
   * neighbors in the conflict graph: `sum(student_conflicts[i].value)`.
   * Used as a heuristic to identify "heavy" exams that are harder to schedule.
   */
  const std::vector<int> total_student_conflicts;

  Evaluator(
    const common::Hyperparams& _hyperparams,
    const std::vector<int64_t>& _slot_timestamps,
    const common::Matrix<int>& _student_conflicts_matrix
  );

  double calculate_delta_penalty(const std::vector<int>& schedule, int exam, int new_slot, int ignore_exam = -1) const;

}; // struct Evaluator

} // namespace common