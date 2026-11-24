#include "slots/scoring.hpp"

#include <cmath>
#include <cstdint>

#include "problem_data.hpp"
#include "slots/eval_params.hpp"
#include "utils/matrix.hpp"
#include "utils/vector.hpp"

namespace cetp::slots {
namespace {

constexpr double SECONDS_PER_DAY = 86400.0;

auto timestamp_distance(int64_t timestamp_1, int64_t timestamp_2) -> uint64_t {
  return timestamp_1 >= timestamp_2 ? uint64_t(timestamp_1) - uint64_t(timestamp_2)
                                    : uint64_t(timestamp_2) - uint64_t(timestamp_1);
}

}  // namespace

// Exponential decay by day gap:
//   exp(-log(base) * seconds / SECONDS_PER_DAY) = base ^ (-days)
auto build_proximity_penalties(const ProblemData& problem, const EvalParams& eval_params)
    -> Matrix<double> {
  const int num_slots = problem.num_slots;
  const Vector<int64_t>& timestamps = problem.slot_timestamps;
  const double decay_base = eval_params.penalty_decay_base();
  const double log_decay_rate = -std::log(decay_base) / SECONDS_PER_DAY;

  Matrix<double> penalties(num_slots, num_slots, INFINITY);
  for (int slot_1 = 0; slot_1 < num_slots; ++slot_1) {
    const int64_t timestamp_1 = timestamps[slot_1];
    for (int slot_2 = slot_1 + 1; slot_2 < num_slots; ++slot_2) {
      const int64_t timestamp_2 = timestamps[slot_2];
      const uint64_t gap_seconds = timestamp_distance(timestamp_1, timestamp_2);
      const double penalty = std::exp(log_decay_rate * double(gap_seconds));
      penalties[slot_1, slot_2] = penalty;
      penalties[slot_2, slot_1] = penalty;
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

  const auto exam_credits = double(problem.exam_credits[exam]);
  const auto [conflict_exams, shared_student_counts] = problem.conflicts_csr[exam];
  double delta_penalty = 0.0;

  for (int neighbor_index = 0; neighbor_index < shared_student_counts.size(); ++neighbor_index) {
    const int conflict_exam = conflict_exams[neighbor_index];
    if (conflict_exam == ignore_exam) {
      continue;
    }
    const int conflict_slot = schedule[conflict_exam];
    if (conflict_slot == -1) {
      continue;
    }

    const auto conflict_credits = double(problem.exam_credits[conflict_exam]);
    const double credit_sum = exam_credits + conflict_credits;
    const double conflict_weight = double(shared_student_counts[neighbor_index]) * credit_sum;
    double penalty_difference = proximity_penalties[new_slot, conflict_slot];
    if (current_slot != -1) {
      penalty_difference -= proximity_penalties[current_slot, conflict_slot];
    }
    delta_penalty += conflict_weight * penalty_difference;
  }
  return delta_penalty;
}

}  // namespace cetp::slots
