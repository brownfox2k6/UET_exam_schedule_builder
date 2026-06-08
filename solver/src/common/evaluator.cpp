#include "common/evaluator.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

#include "common/hyperparameters.hpp"
#include "common/problem_data.hpp"
#include "utils/matrix.hpp"

namespace common {

constexpr double SECONDS_PER_DAY = 86400.0;

Evaluator::Evaluator(const ProblemData& _problem_data, const Hyperparams& _hyperparams)
    : problem_data(_problem_data),
      hyperparams(_hyperparams.eval),
      proximity_penalties(build_proximity_penalties(problem_data, hyperparams)) {}

auto Evaluator::build_proximity_penalties(const ProblemData& problem_data,
                                          const Hyperparams::Evaluation& hyperparams)
    -> utils::Matrix<double> {
  const auto& slot_timestamps = problem_data.slot_timestamps;
  const auto num_slots = static_cast<size_t>(problem_data.num_slots);
  utils::Matrix<double> penalties(num_slots, num_slots, std::numeric_limits<double>::infinity());
  for (size_t i = 0; i < num_slots; ++i) {
    const int64_t slot_i = slot_timestamps[i];
    for (size_t j = i + 1; j < num_slots; ++j) {
      const int64_t slot_j = slot_timestamps[j];
      const double diff_days = std::fabs(slot_i - slot_j) / SECONDS_PER_DAY;
      const double value = std::pow(hyperparams.penalty_decay_base(), -diff_days);
      penalties(i, j) = penalties(j, i) = value;
    }
  }
  return penalties;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
auto Evaluator::calculate_delta_penalty(const std::vector<int>& schedule, int exam, int new_slot,
                                        int ignore_exam) const -> double {
  PANIC_IF(std::cmp_not_equal(schedule.size(), problem_data.num_exams),
           "schedule size mismatch: got {}, expected {}", schedule.size(), problem_data.num_exams);
  PANIC_IF(exam < 0 || exam >= problem_data.num_exams, "exam index out of bounds: {}", exam);
  PANIC_IF(new_slot < 0 || new_slot >= problem_data.num_slots, "new_slot out of bounds: {}",
           new_slot);
  const int cur_slot = schedule[static_cast<size_t>(exam)];
  const auto& credits = problem_data.exam_credits;
  const int exam_credits = credits[static_cast<size_t>(exam)];
  double delta = 0.0;
  const auto [conflict_exams, conflict_counts] = problem_data.conflicts_csrmatrix[exam];
  for (size_t i = 0; i < conflict_counts.size(); ++i) {
    const auto conflict_exam = static_cast<size_t>(conflict_exams[i]);
    const int conflict_slot = schedule[conflict_exam];
    if (std::cmp_equal(conflict_exam, ignore_exam) || conflict_slot == -1) {
      continue;
    }
    const int weight = conflict_counts[i] * (exam_credits + credits[conflict_exam]);
    delta += weight * proximity_penalties(new_slot, conflict_slot);
    if (cur_slot != -1) {
      delta -= weight * proximity_penalties(cur_slot, conflict_slot);
    }
  }
  return delta;
}

}  // namespace common