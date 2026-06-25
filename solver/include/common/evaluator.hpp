#pragma once

#include "hyperparameters.hpp"
#include "problem_data.hpp"
#include "utils/matrix.hpp"

namespace common {

struct Evaluator {
 public:
  Evaluator(const ProblemData& _problem_data, const Hyperparams& _hyperparams);

  [[nodiscard]] auto calculate_delta_penalty(const std::vector<int>& schedule, int exam,
                                             int new_slot, int ignore_exam = -1) const -> double;

 private:
  const ProblemData& problem_data;             // NOLINT(*const-or-ref-data-members*)
  const Hyperparams::Evaluation& hyperparams;  // NOLINT(*const-or-ref-data-members*)

  /**
   * @brief Precalculated time-gap penalties for O(1) lookup.
   * * `proximity_penalties(i, j)` tells the closeness between slot `i` and `j`.
   */
  const utils::Matrix<double> proximity_penalties;  // NOLINT(*const-or-ref-data-members*)

  static auto build_proximity_penalties(const ProblemData& problem_data,
                                        const Hyperparams::Evaluation& hyperparams)
      -> utils::Matrix<double>;

};  // struct Evaluator

}  // namespace common