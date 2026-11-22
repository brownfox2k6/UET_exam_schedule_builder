#pragma once

#include "problem_data.hpp"
#include "slots/eval_params.hpp"
#include "utils/matrix.hpp"
#include "utils/vector.hpp"

namespace cetp::slots {

struct CalculateDeltaPenaltyParams {
  // NOLINTBEGIN(cppcoreguidelines-avoid-const-or-ref-data-members)
  const ProblemData& problem;
  const Matrix<double>& proximity_penalties;
  const Vector<int>& schedule;

  const int exam = -1;
  const int new_slot = -1;
  const int ignore_exam = -1;
  // NOLINTEND(cppcoreguidelines-avoid-const-or-ref-data-members)
};

[[nodiscard]] auto build_proximity_penalties(const ProblemData& problem, const EvalParams& params)
    -> Matrix<double>;

/** Scores an assignment or move using the table built for this problem. */
[[nodiscard]] auto calculate_delta_penalty(const CalculateDeltaPenaltyParams& params) -> double;

}  // namespace cetp::slots
