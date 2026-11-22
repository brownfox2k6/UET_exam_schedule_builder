#include "slots/exam_priority.hpp"

#include <climits>
#include <compare>

#include "problem_data.hpp"
#include "slots/slot_assignment.hpp"

namespace cetp::slots {
namespace {

/** Orders unscheduled exams by remaining slots, conflicts, and enrollment. */
struct ExamPriority {
  // NOLINTBEGIN(*-non-private-*)
  int saturation_degree = INT_MIN;
  int conflict_degree = INT_MIN;
  int weighted_conflict_degree = INT_MIN;
  int student_count = INT_MIN;
  // NOLINTEND(*-non-private-*)

  auto operator<=>(const ExamPriority&) const -> std::strong_ordering = default;
};

}  // namespace

auto select_next_exam(const ProblemData& problem, const SlotAssignment& candidate) -> int {
  // Ordering from van Bulck et al., DOI 10.1007/s10951-023-00799-1.
  int best_exam = -1;
  ExamPriority best_key{};
  for (int exam : problem.exams()) {
    if (candidate.assigned_slots[exam] != -1) {
      continue;
    }
    const ExamPriority key{
        .saturation_degree = -candidate.available_slots_by_exam.get_available_count(exam),
        .conflict_degree = problem.conflicts_csr[exam].size(),
        .weighted_conflict_degree = problem.weighted_conflict_degrees[exam],
        .student_count = problem.exam_student_counts[exam],
    };
    if (key > best_key) {
      best_exam = exam;
      best_key = key;
    }
  }
  return best_exam;
}

}  // namespace cetp::slots
