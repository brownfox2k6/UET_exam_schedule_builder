#include "slots/scoring.hpp"

#include <cmath>
#include <cstdint>

#include "problem_data.hpp"
#include "slots/eval_params.hpp"
#include "utils/matrix.hpp"
#include "utils/vector.hpp"

namespace {

constexpr double SECONDS_PER_DAY = 86400.0;

auto timestamp_distance(int64_t first, int64_t second) -> uint64_t {
  return first >= second ? uint64_t(first) - uint64_t(second) : uint64_t(second) - uint64_t(first);
}

}  // namespace

namespace cetp::slots {

/**
 *     e ^ (-log(base) * seconds / SECONDS_PER_DAY)
 *   = e ^ (-log(base) * days)
 *   = base ^ (-days)
 */
auto build_proximity_penalties(const ProblemData& problem, const EvalParams& params)
    -> Matrix<double> {
  const int num_slots = problem.num_slots;
  const Vector<int64_t>& timestamps = problem.slot_timestamps;
  const double decay_base = params.penalty_decay_base();
  const double log_rate = -std::log(decay_base) / SECONDS_PER_DAY;

  Matrix<double> penalties(num_slots, num_slots, INFINITY);
  for (int i = 0; i < num_slots; ++i) {
    const int64_t timestamp_i = timestamps[i];
    for (int j = i + 1; j < num_slots; ++j) {
      const int64_t timestamp_j = timestamps[j];
      const uint64_t seconds = timestamp_distance(timestamp_i, timestamp_j);
      const double penalty = std::exp(log_rate * double(seconds));
      penalties[i, j] = penalties[j, i] = penalty;
    }
  }
  return penalties;
}

auto calculate_delta_penalty(const CalculateDeltaPenaltyParams& params) -> double {
  const ProblemData& problem = params.problem;
  const Matrix<double>& proximity_penalties = params.proximity_penalties;
  const Vector<int>& schedule = params.schedule;
  const int exam = params.exam;
  const int new_slot = params.new_slot;
  const int ignore_exam = params.ignore_exam;

  const int current_slot = schedule[exam];
  if (new_slot == current_slot) {
    return 0.0;
  }

  const auto current_credits = double(problem.exam_credits[exam]);
  const auto [conflict_exams, conflict_counts] = problem.conflicts_csr[exam];
  double delta = 0.0;

  for (int i = 0; i < conflict_counts.size(); ++i) {
    const int conflict_exam = conflict_exams[i];
    if (conflict_exam == ignore_exam) {
      continue;
    }
    const int conflict_slot = schedule[conflict_exam];
    if (conflict_slot == -1) {
      continue;
    }
    const auto conflict_credits = double(problem.exam_credits[conflict_exam]);
    const double sum_credits = current_credits + conflict_credits;
    const double weight = conflict_counts[i] * sum_credits;
    double penalty_diff = proximity_penalties[new_slot, conflict_slot];
    if (current_slot != -1) {
      penalty_diff -= proximity_penalties[current_slot, conflict_slot];
    }
    delta += weight * penalty_diff;
  }
  return delta;
}

}  // namespace cetp::slots
