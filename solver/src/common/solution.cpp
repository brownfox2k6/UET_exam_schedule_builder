#include "common/solution.hpp"

#include <algorithm>
#include <cstddef>
#include <tuple>

#include "common/problem_data.hpp"
#include "utils/assert.hpp"
#include "utils/feasible_set.hpp"
#include "utils/matrix.hpp"

namespace common {

Solution::Solution(const ProblemData& exams)
    : conflicting_exams_count(exams.num_exams, exams.num_slots, 0),
      assigned_slots(size_t(exams.num_exams), -1),
      feasible_slots(exams.feasible_slots, exams.num_slots),
      feasible_rooms(exams.feasible_rooms, exams.num_rooms),
      penalty(0.0) {}

void Solution::reset() {
  penalty = 0.0;
  feasible_slots.reset();
  feasible_rooms.reset();
  std::ranges::fill(assigned_slots, -1);
  conflicting_exams_count.fill(0);
}

auto Solution::get_next_exam(const ProblemData& problem_data) const -> int {
  /**
   * Implementation based on "Multi-Neighbourhood Simulated Annealing for the ITC-2007 Capacitated
   * Examination Timetabling Problem" by David van Bulck et al. (2025)
   * @see 10.1007/s10951-023-00799-1
   *
   * The following ordering criteria are applied in sequence:
   * (1) Saturation degree - number of feasible slots remaining (fewest first, dynamically updated
   *     after each assignment).
   * (2) Conflict degree - number of conflicting exams (largest first).
   * (3) Weighted conflict degree - total shared students with conflicting exams (largest first).
   * (4) Student count - number of enrolled students (largest first).
   */
  int best_exam = -1;
  std::tuple<int, int, int, int> best_key;
  for (size_t exam = 0; exam < assigned_slots.size(); ++exam) {
    if (assigned_slots[exam] != -1) {
      continue;
    }
    const int saturation_degree = feasible_slots.get_feasible_count(exam);
    const int degree = problem_data.conflicts_csrmatrix[exam].size();
    const int weighted_degree = problem_data.weighted_conflict_degrees[exam];
    const int student_count = problem_data.exam_student_counts[exam];
    std::tuple key = {-saturation_degree, degree, weighted_degree, student_count};
    if (best_exam == -1 || key > best_key) {
      best_exam = int(exam);
      best_key = key;
    }
  }
  return best_exam;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void Solution::assign_exam(int exam, int slot, const utils::CsrMatrix<int>& conflicts_csrmatrix) {
  PANIC_IF(exam < 0 || std::cmp_greater_equal(exam, assigned_slots.size()),
           "Exam index {} out of bounds [0, {}]", exam, assigned_slots.size() - 1);
  PANIC_IF(slot < 0 || std::cmp_greater_equal(slot, conflicting_exams_count.num_cols()),
           "Slot index {} out of bounds [0, {}]", slot, conflicting_exams_count.num_cols() - 1);
  int& current_slot = assigned_slots[size_t(exam)];
  PANIC_IF(current_slot != -1,
           "Exam {} is already scheduled at slot {}, failed to assign to slot {}", exam,
           current_slot, slot);
  PANIC_IF(!feasible_slots.is_active(exam, slot),
           "Exam {} cannot be assigned to inactive/infeasible slot {}", exam, slot);
  for (int conflict_exam : conflicts_csrmatrix[exam].indices) {
    int& count = conflicting_exams_count[conflict_exam, slot];
    if (count++ == 0 && feasible_slots.has_option(conflict_exam, slot)) {
      feasible_slots.remove(conflict_exam, slot);
    }
  }
  current_slot = slot;
}

void Solution::unassign_exam(int exam, const utils::CsrMatrix<int>& conflicts_csrmatrix) {
  PANIC_IF(exam < 0 || std::cmp_greater_equal(exam, assigned_slots.size()),
           "Exam index {} out of bounds [0, {}]", exam, assigned_slots.size() - 1);
  int& current_slot = assigned_slots[size_t(exam)];
  PANIC_IF(current_slot == -1, "Exam {} is not scheduled yet", exam);
  for (int conflict_exam : conflicts_csrmatrix[exam].indices) {
    int& count = conflicting_exams_count[conflict_exam, current_slot];
    PANIC_IF(count <= 0, "Invalid conflict count for exam {} at slot {}: {}", exam, current_slot,
             count);
    if (--count == 0 && feasible_slots.has_option(conflict_exam, current_slot)) {
      feasible_slots.restore(conflict_exam, current_slot);
    }
  }
  current_slot = -1;
}

}  // namespace common