#include "common/slot_assignment.hpp"

#include <cstddef>
#include <utility>

#include "common/exam_priority.hpp"
#include "common/problem_data.hpp"
#include "utils/assert.hpp"
#include "utils/available_set.hpp"
#include "utils/matrix.hpp"

namespace common {

SlotAssignment::SlotAssignment(const ProblemData& problem_data)
    : conflicting_exams_count(problem_data.num_exams, problem_data.num_slots, 0),
      assigned_slots(size_t(problem_data.num_exams), -1),
      available_slots_by_exam(problem_data.feasible_slots_by_exam, problem_data.num_slots) {}

auto SlotAssignment::get_next_exam(const ProblemData& problem_data) const -> int {
  int best_exam = -1;
  ExamPriority best_key{};
  for (size_t exam = 0; exam < assigned_slots.size(); ++exam) {
    if (assigned_slots[exam] != -1) {
      continue;
    }
    ExamPriority key{
        .saturation_degree = -available_slots_by_exam.get_available_count(exam),
        .conflict_degree = problem_data.conflicts_csrmatrix[exam].size(),
        .weighted_conflict_degree = problem_data.weighted_conflict_degrees[exam],
        .student_count = problem_data.exam_student_counts[exam],
    };
    if (key > best_key) {
      best_exam = int(exam);
      best_key = key;
    }
  }
  return best_exam;
}

void SlotAssignment::assign_exam(int exam, int slot,
                                 const utils::CsrMatrix<int>& conflicts_csrmatrix) {
  PANIC_IF(exam < 0 || std::cmp_greater_equal(exam, assigned_slots.size()),
           "Exam index {} out of bounds [0, {}]", exam, assigned_slots.size() - 1);
  PANIC_IF(slot < 0 || std::cmp_greater_equal(slot, conflicting_exams_count.num_cols()),
           "Slot index {} out of bounds [0, {}]", slot, conflicting_exams_count.num_cols() - 1);
  int& current_slot = assigned_slots[size_t(exam)];
  PANIC_IF(current_slot != -1,
           "Exam {} is already scheduled at slot {}, failed to assign to slot {}", exam,
           current_slot, slot);
  PANIC_IF(!available_slots_by_exam.is_available(exam, slot),
           "Exam {} cannot be assigned to inactive/infeasible slot {}", exam, slot);
  for (int conflict_exam : conflicts_csrmatrix[exam].indices) {
    int& count = conflicting_exams_count[conflict_exam, slot];
    if (count++ == 0 && available_slots_by_exam.has_option(conflict_exam, slot)) {
      available_slots_by_exam.remove(conflict_exam, slot);
    }
  }
  current_slot = slot;
}

void SlotAssignment::unassign_exam(int exam, const utils::CsrMatrix<int>& conflicts_csrmatrix) {
  PANIC_IF(exam < 0 || std::cmp_greater_equal(exam, assigned_slots.size()),
           "Exam index {} out of bounds [0, {}]", exam, assigned_slots.size() - 1);
  int& current_slot = assigned_slots[size_t(exam)];
  PANIC_IF(current_slot == -1, "Exam {} is not scheduled yet", exam);
  for (int conflict_exam : conflicts_csrmatrix[exam].indices) {
    int& count = conflicting_exams_count[conflict_exam, current_slot];
    PANIC_IF(count <= 0, "Invalid conflict count for exam {} at slot {}: {}", exam, current_slot,
             count);
    if (--count == 0 && available_slots_by_exam.has_option(conflict_exam, current_slot)) {
      available_slots_by_exam.restore(conflict_exam, current_slot);
    }
  }
  current_slot = -1;
}

void SlotAssignment::reset() {
  conflicting_exams_count.fill(0);
  std::ranges::fill(assigned_slots, -1);
  available_slots_by_exam.reset();
}

}  // namespace common
