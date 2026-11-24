#include "slots/local_search/local_search.hpp"

#include <cstdint>
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
#include "utils/random.hpp"
#include "utils/vector.hpp"

namespace cetp::slots::local_search {
namespace {

constexpr double EPSILON = 1e-12;

class LocalSearch {
public:
  LocalSearch(
      const ProblemData& problem,
      const Params& params,
      const Matrix<double>& proximity_penalties,
      uint64_t base_seed
  )
      : problem_(problem),
        proximity_penalties_(proximity_penalties),
        params_(params),
        candidate_(problem),
        random_engine_(make_random_engine(base_seed)) {}

  [[nodiscard]] auto run(const Vector<int>& schedule) -> SlotResult {
    initialize_schedule(schedule);
    int improvements = 0;
    int consecutive_failures = 0;
    while (improvements < params_.max_improvements() && consecutive_failures < params_.patience()) {
      const bool use_move = random_double(random_engine_, 0.0, 1.0) < params_.prob_1_move();
      const bool accepted = use_move ? try_move() : try_swap();
      if (accepted) {
        ++improvements;
        consecutive_failures = 0;
      } else {
        ++consecutive_failures;
      }
    }
    return {.schedule = std::move(candidate_.assigned_slots), .penalty = candidate_.penalty};
  }

private:
  // NOLINTBEGIN(cppcoreguidelines-avoid-const-or-ref-data-members)
  const ProblemData& problem_;
  const Matrix<double>& proximity_penalties_;
  // NOLINTEND(cppcoreguidelines-avoid-const-or-ref-data-members)
  Params params_;
  SlotAssignment candidate_;
  std::mt19937 random_engine_;

  void initialize_schedule(const Vector<int>& schedule) {
    for (int exam : problem_.exams()) {
      const int slot = schedule[exam];
      candidate_.penalty += calculate_delta_penalty(
          {.problem = problem_,
           .proximity_penalties = proximity_penalties_,
           .schedule = candidate_.assigned_slots,
           .exam = exam,
           .new_slot = slot}
      );
      candidate_.assign_exam(exam, slot, problem_.conflicts_csr);
    }
  }

  [[nodiscard]] auto try_move() -> bool {
    const int exam = random_int(random_engine_, 0, problem_.num_exams);
    const int available_count = candidate_.available_slots_by_exam.get_available_count(exam);
    if (available_count <= 1) {
      return false;
    }
    const int new_slot = candidate_.available_slots_by_exam.get_random(exam, random_engine_);
    if (new_slot == candidate_.assigned_slots[exam]) {
      return false;
    }
    const double delta = calculate_delta_penalty(
        {.problem = problem_,
         .proximity_penalties = proximity_penalties_,
         .schedule = candidate_.assigned_slots,
         .exam = exam,
         .new_slot = new_slot}
    );
    if (delta >= -EPSILON) {
      return false;
    }

    candidate_.unassign_exam(exam, problem_.conflicts_csr);
    candidate_.assign_exam(exam, new_slot, problem_.conflicts_csr);
    candidate_.penalty += delta;
    return true;
  }

  [[nodiscard]] auto can_swap_exams(int exam_1, int exam_2) const -> bool {
    const int slot_1 = candidate_.assigned_slots[exam_1];
    const int slot_2 = candidate_.assigned_slots[exam_2];
    if (slot_1 == slot_2) {
      return false;
    }
    if (!candidate_.available_slots_by_exam.has_option(exam_1, slot_2)
        || !candidate_.available_slots_by_exam.has_option(exam_2, slot_1)) {
      return false;
    }

    // Both exams vacate their current slots simultaneously, so a direct conflict between them does
    // not block the swap.
    int conflicts_in_slot_1 = candidate_.conflicting_exams_count[exam_2, slot_1];
    int conflicts_in_slot_2 = candidate_.conflicting_exams_count[exam_1, slot_2];
    if (problem_.conflicts_matrix[exam_1, exam_2] > 0) {
      --conflicts_in_slot_1;
      --conflicts_in_slot_2;
    }
    return conflicts_in_slot_1 == 0 && conflicts_in_slot_2 == 0;
  }

  [[nodiscard]] auto calculate_swap_delta(int exam_1, int exam_2) const -> double {
    const Vector<int>& schedule = candidate_.assigned_slots;
    const int slot_1 = schedule[exam_1];
    const int slot_2 = schedule[exam_2];

    // Ignore the partner exam on both sides: their mutual time distance is symmetric and unchanged.
    const double delta_1 = calculate_delta_penalty(
        {.problem = problem_,
         .proximity_penalties = proximity_penalties_,
         .schedule = schedule,
         .exam = exam_1,
         .new_slot = slot_2,
         .ignore_exam = exam_2}
    );
    const double delta_2 = calculate_delta_penalty(
        {.problem = problem_,
         .proximity_penalties = proximity_penalties_,
         .schedule = schedule,
         .exam = exam_2,
         .new_slot = slot_1,
         .ignore_exam = exam_1}
    );
    return delta_1 + delta_2;
  }

  void apply_swap(int exam_1, int exam_2, double delta) {
    const int slot_1 = candidate_.assigned_slots[exam_1];
    const int slot_2 = candidate_.assigned_slots[exam_2];
    candidate_.unassign_exam(exam_1, problem_.conflicts_csr);
    candidate_.unassign_exam(exam_2, problem_.conflicts_csr);
    candidate_.assign_exam(exam_1, slot_2, problem_.conflicts_csr);
    candidate_.assign_exam(exam_2, slot_1, problem_.conflicts_csr);
    candidate_.penalty += delta;
  }

  [[nodiscard]] auto try_swap() -> bool {
    const int exam_1 = random_int(random_engine_, 0, problem_.num_exams);
    const int exam_2 = random_int(random_engine_, 0, problem_.num_exams);
    if (exam_1 == exam_2 || !can_swap_exams(exam_1, exam_2)) {
      return false;
    }
    const double delta = calculate_swap_delta(exam_1, exam_2);
    if (delta >= -EPSILON) {
      return false;
    }
    apply_swap(exam_1, exam_2, delta);
    return true;
  }
};

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
  return LocalSearch(problem, params, proximity_penalties, base_seed).run(schedule);
}

}  // namespace cetp::slots::local_search
