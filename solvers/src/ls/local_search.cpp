#include "ls/local_search.hpp"

#include <cstddef>
#include <utility>

#include "utils/assert.hpp"

namespace ls {

LocalSearch::LocalSearch(common::Hyperparams::Evaluation evaluation_params,
                         common::Hyperparams::LS hyperparams,
                         const std::vector<common::Exam>& exams,
                         std::vector<int64_t> slot_timestamps,
                         const std::vector<common::Room>& rooms, uint64_t base_seed)
    : evaluation_params_(evaluation_params),
      hyperparams_(hyperparams),
      problem_data_(exams, std::move(slot_timestamps), rooms),
      evaluator_(problem_data_, evaluation_params_),
      solution_(problem_data_) {
  std::seed_seq seed{uint32_t(base_seed), uint32_t(base_seed >> 32)};  // NOLINT(*-magic-numbers)
  rng_.seed(seed);
}

void LocalSearch::initialize_solution(const std::vector<int>& initial_schedule) {
  PANIC_IF(initial_schedule.size() != size_t(problem_data_.num_exams),
           "Initial schedule size mismatch: got {}, expected {}", initial_schedule.size(),
           problem_data_.num_exams);

  solution_.reset();
  for (int exam = 0; exam < problem_data_.num_exams; ++exam) {
    const int slot = initial_schedule[size_t(exam)];
    PANIC_IF(slot < 0 || slot >= problem_data_.num_slots,
             "Initial schedule has invalid slot {} for exam {}", slot, exam);
    PANIC_IF(!solution_.feasible_slots.is_active(exam, slot),
             "Initial schedule violates a hard constraint for exam {} at slot {}", exam, slot);

    solution_.penalty += evaluator_.calculate_delta_penalty(solution_.assigned_slots, exam, slot);
    solution_.assign_exam(exam, slot, problem_data_.conflicts_csrmatrix);
  }
}

auto LocalSearch::go_1_move() -> bool {
  const int exam = std::uniform_int_distribution<int>(0, problem_data_.num_exams - 1)(rng_);
  const int count_feasible = solution_.feasible_slots.get_feasible_count(exam);
  if (count_feasible == 0) {
    return false;
  }

  const int new_slot = solution_.feasible_slots.get_random(exam, rng_);
  if (new_slot == solution_.assigned_slots[size_t(exam)]) {
    return false;
  }

  const double delta = evaluator_.calculate_delta_penalty(solution_.assigned_slots, exam, new_slot);
  if (delta > 0.0) {
    return false;
  }

  solution_.unassign_exam(exam, problem_data_.conflicts_csrmatrix);
  solution_.assign_exam(exam, new_slot, problem_data_.conflicts_csrmatrix);
  solution_.penalty += delta;
  return true;
}

auto LocalSearch::go_2_swap() -> bool {
  const int exam1 = std::uniform_int_distribution<int>(0, problem_data_.num_exams - 1)(rng_);
  const int exam2 = std::uniform_int_distribution<int>(0, problem_data_.num_exams - 1)(rng_);
  const int slot1 = solution_.assigned_slots[size_t(exam1)];
  const int slot2 = solution_.assigned_slots[size_t(exam2)];
  if (exam1 == exam2 || slot1 == slot2) {
    return false;
  }
  if (!solution_.feasible_slots.has_option(exam1, slot2) ||
      !solution_.feasible_slots.has_option(exam2, slot1)) {
    return false;
  }

  int conflicts_in_slot1 = solution_.conflicting_exams_count[exam2, slot1];
  int conflicts_in_slot2 = solution_.conflicting_exams_count[exam1, slot2];
  if (problem_data_.conflicts_matrix[exam1, exam2] > 0) {
    --conflicts_in_slot1;
    --conflicts_in_slot2;
  }
  if (conflicts_in_slot1 > 0 || conflicts_in_slot2 > 0) {
    return false;
  }

  const double delta =
      evaluator_.calculate_delta_penalty(solution_.assigned_slots, exam1, slot2, exam2) +
      evaluator_.calculate_delta_penalty(solution_.assigned_slots, exam2, slot1, exam1);
  if (delta > 0.0) {
    return false;
  }

  solution_.unassign_exam(exam1, problem_data_.conflicts_csrmatrix);
  solution_.unassign_exam(exam2, problem_data_.conflicts_csrmatrix);
  solution_.assign_exam(exam1, slot2, problem_data_.conflicts_csrmatrix);
  solution_.assign_exam(exam2, slot1, problem_data_.conflicts_csrmatrix);
  solution_.penalty += delta;
  return true;
}

auto LocalSearch::run(const std::vector<int>& initial_schedule) -> common::Solution {
  const std::scoped_lock lock(execution_mutex_);
  initialize_solution(initial_schedule);

  int improvements = 0;
  int consecutive_fails = 0;
  while ((improvements < hyperparams_.max_improvements()) &&
         (consecutive_fails < hyperparams_.patience())) {
    const auto is_accepted = [&]() -> bool {
      const double random = std::uniform_real_distribution<double>(0.0, 1.0)(rng_);
      if (random < hyperparams_.prob_1_move()) {
        return go_1_move();
      }
      return go_2_swap();
    }();

    if (is_accepted) {
      ++improvements;
      consecutive_fails = 0;
    } else {
      ++consecutive_fails;
    }
  }
  return solution_;
}

}  // namespace ls