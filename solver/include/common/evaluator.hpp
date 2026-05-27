#pragma once

#include "exams.hpp"
#include "hyperparameters.hpp"
#include "utils/matrix.hpp"

constexpr double SECONDS_PER_DAY = 86400.0;
constexpr double HARD_CONSTRAINT_PENALTY = 1e9;

namespace common {

struct Evaluator {

public:
  double calculate_delta_penalty(const std::vector<int>& schedule, int exam, int new_slot, int ignore_exam = -1) const;
  
  Evaluator(
    const Hyperparams& _hyperparams,
    const Exams& _exams,
    const std::vector<int64_t>& _slot_timestamps,
    int num_rooms
  );
  
private:
  const Hyperparams::Evaluation& hyperparams;
  const Exams& exams;
  
  /**
   * @brief Precalculated time-gap penalties for O(1) lookup.
   * * `proximity_penalties(i, j)` tells the closeness between slot `i` and `j`.
   */
  const utils::Matrix<double> proximity_penalties;

  static utils::Matrix<double> build_proximity_penalties(
    const std::vector<int64_t>& slot_timestamps,
    const Hyperparams::Evaluation& hyperparams
  );

}; // struct Evaluator

} // namespace common