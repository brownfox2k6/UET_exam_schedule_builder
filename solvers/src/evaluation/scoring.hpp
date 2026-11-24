#pragma once

#include "evaluation/params.hpp"
#include "problem_data.hpp"
#include "utils/matrix.hpp"
#include "utils/vector.hpp"

namespace cetp::evaluation {

struct CalculateDeltaPenaltyParams {
  // NOLINTBEGIN(cppcoreguidelines-avoid-const-or-ref-data-members)
  const ProblemData& problem;
  const Matrix<double>& proximity_penalties;
  const Vector<int>& schedule;
  // NOLINTEND(cppcoreguidelines-avoid-const-or-ref-data-members)

  int exam = -1;
  int new_slot = -1;
  int ignore_exam = -1;
};

/** Precomputes pairwise time-proximity penalties between every pair of slots. */
[[nodiscard]] auto build_proximity_penalties(const ProblemData& problem, const Params& params)
    -> Matrix<double>;

/** Scores an assignment or move using the proximity penalty table built for this problem. */
[[nodiscard]] auto calculate_delta_penalty(const CalculateDeltaPenaltyParams& params) -> double;

}  // namespace cetp::evaluation
