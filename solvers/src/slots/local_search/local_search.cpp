#include "slots/local_search/local_search.hpp"

#include <cstdint>
#include <functional>
#include <random>
#include <utility>

#include "problem_data.hpp"
#include "slots/eval_params.hpp"
#include "slots/local_search/params.hpp"
#include "slots/scoring.hpp"
#include "slots/slot_assignment.hpp"
#include "slots/slot_result.hpp"
#include "timetable.hpp"
#include "utils/matrix.hpp"
#include "utils/vector.hpp"

namespace cetp::slots::local_search {
namespace {

constexpr double EPSILON = 1e-12;

class LocalSearch {
public:
  LocalSearch(const ProblemData& problem, uint64_t base_seed);

  auto run(
      const Vector<int>& schedule,
      const Params& params,
      const Matrix<double>& proximity_penalties
  ) -> SlotResult;

private:
  std::reference_wrapper<const ProblemData> problem_;
  SlotAssignment candidate_;
  std::mt19937 rng_;

  void initialize_schedule(const Vector<int>& schedule, const Matrix<double>& proximity_penalties);
  auto try_move(const Matrix<double>& proximity_penalties) -> bool;
  auto try_swap(const Matrix<double>& proximity_penalties) -> bool;
};

LocalSearch::LocalSearch(const ProblemData& problem, uint64_t base_seed)
    : problem_(problem),
      candidate_(problem) {
  std::seed_seq seed{uint32_t(base_seed), uint32_t(base_seed >> 32)};  // NOLINT(*-magic-numbers)
  rng_.seed(seed);
}

void LocalSearch::initialize_schedule(
    const Vector<int>& schedule,
    const Matrix<double>& proximity_penalties
) {
  const ProblemData& problem = problem_.get();
  for (int exam : problem.exams()) {
    const int slot = schedule[exam];
    candidate_.penalty += calculate_delta_penalty(
        {.problem = problem,
         .proximity_penalties = proximity_penalties,
         .schedule = candidate_.assigned_slots,
         .exam = exam,
         .new_slot = slot}
    );
    candidate_.assign_exam(exam, slot, problem.conflicts_csr);
  }
}

auto LocalSearch::try_move(const Matrix<double>& proximity_penalties) -> bool {
  const ProblemData& problem = problem_.get();

  std::uniform_int_distribution<int> dist(0, problem.num_exams - 1);
  const int exam = dist(rng_);
  const int count_available = candidate_.available_slots_by_exam.get_available_count(exam);
  if (count_available == 0) {
    return false;
  }

  const int new_slot = candidate_.available_slots_by_exam.get_random(exam, rng_);
  if (new_slot == candidate_.assigned_slots[exam]) {
    return false;
  }

  const double delta = calculate_delta_penalty(
      {.problem = problem,
       .proximity_penalties = proximity_penalties,
       .schedule = candidate_.assigned_slots,
       .exam = exam,
       .new_slot = new_slot}
  );
  if (delta >= -EPSILON) {
    return false;
  }

  candidate_.unassign_exam(exam, problem.conflicts_csr);
  candidate_.assign_exam(exam, new_slot, problem.conflicts_csr);
  candidate_.penalty += delta;
  return true;
}

auto LocalSearch::try_swap(const Matrix<double>& proximity_penalties) -> bool {
  const ProblemData& problem = problem_.get();

  std::uniform_int_distribution<int> dist(0, problem.num_exams - 1);
  const int exam1 = dist(rng_);
  const int exam2 = dist(rng_);
  if (exam1 == exam2) {
    return false;
  }

  const auto& schedule = candidate_.assigned_slots;
  const int slot1 = schedule[exam1];
  const int slot2 = schedule[exam2];
  if (slot1 == slot2) {
    return false;
  }

  if (!candidate_.available_slots_by_exam.has_option(exam1, slot2)
      || !candidate_.available_slots_by_exam.has_option(exam2, slot1)) {
    return false;
  }

  int conflicts_in_slot1 = candidate_.conflicting_exams_count[exam2, slot1];
  int conflicts_in_slot2 = candidate_.conflicting_exams_count[exam1, slot2];
  if (problem.conflicts_matrix[exam1, exam2] > 0) {
    --conflicts_in_slot1;
    --conflicts_in_slot2;
  }
  if (conflicts_in_slot1 > 0 || conflicts_in_slot2 > 0) {
    return false;
  }

  const double delta_1 = calculate_delta_penalty(
      {.problem = problem,
       .proximity_penalties = proximity_penalties,
       .schedule = schedule,
       .exam = exam1,
       .new_slot = slot2,
       .ignore_exam = exam2}
  );
  const double delta_2 = calculate_delta_penalty(
      {.problem = problem,
       .proximity_penalties = proximity_penalties,
       .schedule = schedule,
       .exam = exam2,
       .new_slot = slot1,
       .ignore_exam = exam1}
  );
  const double delta = delta_1 + delta_2;
  if (delta >= -EPSILON) {
    return false;
  }

  candidate_.unassign_exam(exam1, problem.conflicts_csr);
  candidate_.unassign_exam(exam2, problem.conflicts_csr);
  candidate_.assign_exam(exam1, slot2, problem.conflicts_csr);
  candidate_.assign_exam(exam2, slot1, problem.conflicts_csr);
  candidate_.penalty += delta;
  return true;
}

auto LocalSearch::run(
    const Vector<int>& schedule,
    const Params& params,
    const Matrix<double>& proximity_penalties
) -> SlotResult {
  initialize_schedule(schedule, proximity_penalties);
  int improvements = 0;
  int consecutive_fails = 0;
  while ((improvements < params.max_improvements()) && (consecutive_fails < params.patience())) {
    const double random = std::uniform_real_distribution<double>(0.0, 1.0)(rng_);
    const bool do_move = random < params.prob_1_move();
    const bool is_accepted =
        do_move ? try_move(proximity_penalties) : try_swap(proximity_penalties);

    if (is_accepted) {
      ++improvements;
      consecutive_fails = 0;
    } else {
      ++consecutive_fails;
    }
  }
  return {std::move(candidate_.assigned_slots), candidate_.penalty};
}

}  // namespace

auto improve(
    const ProblemData& problem,
    const Vector<int>& schedule,
    Params params,
    EvalParams eval_params,
    uint64_t base_seed
) -> SlotResult {
  validate_schedule(problem, schedule);
  const auto proximity_penalties = build_proximity_penalties(problem, eval_params);
  return LocalSearch(problem, base_seed).run(schedule, params, proximity_penalties);
}

}  // namespace cetp::slots::local_search
