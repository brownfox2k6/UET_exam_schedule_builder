#include "common/evaluator.hpp"
#include "common/hyperparameters.hpp"
#include "common/problem_data.hpp"
#include "utils/matrix.hpp"

namespace common {

Evaluator::Evaluator(
  const ProblemData& _problem_data,
  const Hyperparams& _hyperparams
):
  problem_data(_problem_data),
  hyperparams(_hyperparams.eval),
  proximity_penalties(build_proximity_penalties(problem_data, hyperparams))
{}

utils::Matrix<double> Evaluator::build_proximity_penalties(
  const ProblemData& problem_data,
  const Hyperparams::Evaluation& hyperparams
) {
  const auto& slot_timestamps = problem_data.slot_timestamps;
  const int num_slots = problem_data.num_slots;
  utils::Matrix<double> penalties(num_slots, num_slots, HARD_CONSTRAINT_PENALTY);
  for (int i = 0; i < num_slots; ++i) {
    const int64_t slot_i = slot_timestamps[i];
    for (int j = i + 1; j < num_slots; ++j) {
      const double diff_days = std::abs(slot_i - slot_timestamps[j]) / SECONDS_PER_DAY;
      const double value = std::pow(hyperparams.penalty_decay_base(), -diff_days);
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
  PANIC_IF(
    exam < 0 || exam >= problem_data.num_exams,
    "Evaluator::calculate_delta_penalty: exam index out of bounds: {}", exam
  );
  PANIC_IF(
    new_slot < 0 || new_slot >= problem_data.num_slots,
    "Evaluator::calculate_delta_penalty: new_slot out of bounds: {}", new_slot
  );
  PANIC_IF(
    schedule.size() != static_cast<size_t>(problem_data.num_exams),
    "Evaluator::calculate_delta_penalty: schedule size mismatch: got {}, expected {}",
    schedule.size(), problem_data.num_exams
  );
  const int cur_slot = schedule[exam];
  const int exam_credits = problem_data.exam_credits[exam];
  double delta = 0.0;
  const auto row_view = problem_data.conflicts_csrmatrix[exam];
  for (int i = 0; i < row_view.size(); ++i) {
    const int conflict_exam = row_view.indices[i];
    const int conflict_count = row_view.values[i];
    const int conflict_slot = schedule[conflict_exam];
    if (conflict_exam == ignore_exam || conflict_slot == -1) {
      continue;
    }
    const int weight = conflict_count * (exam_credits + problem_data.exam_credits[conflict_exam]);
    delta += weight * proximity_penalties(new_slot, conflict_slot);
    if (cur_slot != -1) {
      delta -= weight * proximity_penalties(cur_slot, conflict_slot);
    }
  }
  return delta;
}

} // namespace common