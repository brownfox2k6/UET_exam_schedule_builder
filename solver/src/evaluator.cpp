#include "common/evaluator.hpp"

#include <numeric>

constexpr double SECONDS_PER_DAY = 86400.0;
constexpr double HARD_CONSTRAINT_PENALTY = 1e9;

namespace common {

Evaluator::Evaluator(
  const common::Hyperparams& hp,
  const std::vector<int64_t>& slot_timestamps,
  const common::Matrix<int>& student_conflicts_matrix
):
  hyperparams(hp.eval),
  num_exams(student_conflicts_matrix.num_rows()),
  num_slots(slot_timestamps.size()),
  proximity_penalties([&]() {
    common::Matrix<double> penalties(num_slots, num_slots, 0.0);
    for (int i = 0; i < num_slots; ++i) {
      penalties(i, i) = HARD_CONSTRAINT_PENALTY;
      const int64_t slot_i = slot_timestamps[i];
      for (int j = i + 1; j < num_slots; ++j) {
        const double diff_days = std::abs(slot_i - slot_timestamps[j]) / SECONDS_PER_DAY;
        const double value = std::pow(hyperparams.penalty_decay_base, -diff_days);
        penalties(i, j) = penalties(j, i) = value;
      }
    }
    return penalties;
  }()),
  student_conflicts_matrix(student_conflicts_matrix),
  student_conflicts(student_conflicts_matrix),
  total_student_conflicts([&]() {
    std::vector<int> totals(num_exams);
    for (int exam = 0; exam < num_exams; ++exam) {
      const auto row = student_conflicts[exam];
      totals[exam] = std::accumulate(row.begin(), row.end(), 0);
    }
    return totals;
  }())
{}

double Evaluator::calculate_delta_penalty(
  const std::vector<int>& schedule,
  int exam,
  int new_slot,
  int ignore_exam
) const {
  const int cur_slot = schedule[exam];
  double delta = 0.0;
  for (const auto& [conflict_exam, weight] : student_conflicts[exam]) {
    const int conflict_slot = schedule[conflict_exam];
    if (conflict_exam == ignore_exam || conflict_slot == -1) {
      continue;
    }
    delta += weight * proximity_penalties(new_slot, conflict_slot);
    if (cur_slot != -1) {
      delta -= weight * proximity_penalties(cur_slot, conflict_slot);
    }
  }
  return delta;
}

} // namespace common