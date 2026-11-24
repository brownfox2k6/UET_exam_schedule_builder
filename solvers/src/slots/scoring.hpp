#pragma once

#include "problem_data.hpp"
#include "slots/eval_params.hpp"
#include "utils/matrix.hpp"
#include "utils/vector.hpp"

namespace cetp::slots {

struct CalculateDeltaPenaltyParams {
  // NOLINTBEGIN(*-non-private-*, cppcoreguidelines-avoid-const-or-ref-data-members)
  const ProblemData& problem;
  const Matrix<double>& proximity_penalties;
  const Vector<int>& schedule;

  int exam = -1;
  int new_slot = -1;
  int ignore_exam = -1;
  // NOLINTEND(*-non-private-*, cppcoreguidelines-avoid-const-or-ref-data-members)
};

/** Precomputes pairwise time-proximity penalties between every pair of slots. */
[[nodiscard]] auto build_proximity_penalties(
    const ProblemData& problem,
    const EvalParams& eval_params
) -> Matrix<double>;

/** Scores an assignment or move using the proximity penalty table built for this problem. */
[[nodiscard]] auto calculate_delta_penalty(const CalculateDeltaPenaltyParams& params) -> double;

}  // namespace cetp::slots
