#include <vector>

#include "common/evaluator.hpp"
#include "common/hyperparameters.hpp"
#include "utils/matrix.hpp"

namespace common {

Evaluator::Evaluator(
  const Hyperparams& _hyperparams,
  const Exams& _exams,
  const std::vector<int64_t>& _slot_timestamps,
  int _num_rooms
):
  hyperparams(_hyperparams.eval),
  exams(_exams),
  proximity_penalties(build_proximity_penalties(_slot_timestamps, hyperparams))
{}

utils::Matrix<double> Evaluator::build_proximity_penalties(
  const std::vector<int64_t>& slot_timestamps,
  const Hyperparams::Evaluation& hyperparams
) {
  const int num_slots = slot_timestamps.size();
  utils::Matrix<double> penalties(num_slots, num_slots, HARD_CONSTRAINT_PENALTY);
  for (int i = 0; i < num_slots; ++i) {
    const int64_t slot_i = slot_timestamps[i];
    for (int j = i + 1; j < num_slots; ++j) {
      const double diff_days = std::abs(slot_i - slot_timestamps[j]) / SECONDS_PER_DAY;
      const double value = std::pow(hyperparams.penalty_decay_base, -diff_days);
      penalties(i, j) = penalties(j, i) = value;
    }
  }
  return penalties;
}

double Evaluator::calculate_delta_penalty(
  const std::vector<int>& schedule,
  int exam,
  int new_slot,
  int ignore_exam
) const {
  const int cur_slot = schedule[exam];
  const int exam_credits = exams.credits[exam];
  double delta = 0.0;
  const auto row_view = exams.conflicts_csrmatrix[exam];
  for (int i = 0; i < row_view.size(); ++i) {
    int conflict_exam = row_view.indices[i];
    int conflict_count = row_view.values[i];
    const int conflict_slot = schedule[conflict_exam];
    if (conflict_exam == ignore_exam || conflict_slot == -1) {
      continue;
    }
    const int weight = conflict_count * (exam_credits + exams.credits[conflict_exam]);
    delta += weight * proximity_penalties(new_slot, conflict_slot);
    if (cur_slot != -1) {
      delta -= weight * proximity_penalties(cur_slot, conflict_slot);
    }
  }
  return delta;
}

} // namespace common