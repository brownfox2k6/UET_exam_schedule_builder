#pragma once

#include "hyperparameters.hpp"
#include "problem_data.hpp"
#include "utils/matrix.hpp"

constexpr double SECONDS_PER_DAY = 86400.0;
constexpr double HARD_CONSTRAINT_PENALTY = 1e9;

namespace common {

struct Evaluator {
 public:
  Evaluator(const ProblemData& _problem_data, const Hyperparams& _hyperparams);

  [[nodiscard]] auto calculate_delta_penalty(const std::vector<int>& schedule, int exam,
                                             int new_slot, int ignore_exam = -1) const -> double;

 private:
  const ProblemData& problem_data;
  const Hyperparams::Evaluation& hyperparams;

  /**
   * @brief Precalculated time-gap penalties for O(1) lookup.
   * * `proximity_penalties(i, j)` tells the closeness between slot `i` and `j`.
   */
  const utils::Matrix<double> proximity_penalties;

  static auto build_proximity_penalties(const ProblemData& problem_data,
                                        const Hyperparams::Evaluation& hyperparams)
      -> utils::Matrix<double>;

};  // struct Evaluator

}  // namespace common