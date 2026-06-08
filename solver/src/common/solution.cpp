#include "common/solution.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>

#include "common/problem_data.hpp"
#include "utils/assert.hpp"
#include "utils/feasible_set.hpp"
#include "utils/matrix.hpp"

namespace common {

Solution::Solution(const ProblemData& exams)
    : conflicting_exams_count(exams.num_exams, exams.num_slots, 0),
      assigned_slots(static_cast<size_t>(exams.num_exams), -1),
      feasible_slots(exams.feasible_slots, exams.num_slots),
      feasible_rooms(exams.feasible_rooms, exams.num_rooms),
      fitness(0.0) {}

auto Solution::operator<(const Solution& other) const -> bool { return fitness < other.fitness; }

void Solution::reset() {
  fitness = 0.0;
  feasible_slots.reset();
  feasible_rooms.reset();
  std::ranges::fill(assigned_slots, -1);
  conflicting_exams_count.fill(0);
}

auto Solution::get_next_exam(const ProblemData& problem_data) const -> int {
  int best_exam = -1;
  int max_degree = -1;
  int max_conflict = -1;
  for (size_t exam = 0; exam < assigned_slots.size(); ++exam) {
    if (assigned_slots[exam] != -1) {
      continue;
    }
    int degree = feasible_slots.get_forbidden_count(static_cast<int>(exam));
    int conflict = problem_data.weighted_conflict_degrees[exam];
    if (std::tie(degree, conflict) > std::tie(max_degree, max_conflict)) {
      max_degree = degree;
      max_conflict = conflict;
      best_exam = static_cast<int>(exam);
    }
  }
  return best_exam;
}

void Solution::assign_exam(int exam, int slot, const utils::CsrMatrix<int>& conflicts_csrmatrix) {
  PANIC_IF(exam < 0 || std::cmp_greater_equal(exam, assigned_slots.size()),
           "Exam index {} out of bounds [0, {}]", exam, assigned_slots.size() - 1);
  PANIC_IF(slot < 0 || std::cmp_greater_equal(slot, conflicting_exams_count.num_cols()),
           "Slot index {} out of bounds [0, {}]", exam, conflicting_exams_count.num_cols() - 1);
  int& current_slot = assigned_slots[static_cast<size_t>(exam)];
  PANIC_IF(current_slot != -1,
           "Exam {} is already scheduled at slot {}, failed to assign to slot {}", exam,
           current_slot, slot);
  PANIC_IF(!feasible_slots.is_active(exam, slot),
           "Exam {} cannot be assigned to inactive/infeasible slot {}", exam, slot);
  for (int conflict_exam : conflicts_csrmatrix[exam].indices) {
    int& count = conflicting_exams_count(conflict_exam, slot);
    if (count++ == 0 && feasible_slots.has_option(conflict_exam, slot)) {
      feasible_slots.remove(conflict_exam, slot);
    }
  }
  current_slot = slot;
}

void Solution::unassign_exam(int exam, const utils::CsrMatrix<int>& conflicts_csrmatrix) {
  PANIC_IF(exam < 0 || std::cmp_greater_equal(exam, assigned_slots.size()),
           "Exam index {} out of bounds [0, {}]", exam, assigned_slots.size() - 1);
  int& current_slot = assigned_slots[static_cast<size_t>(exam)];
  PANIC_IF(current_slot == -1, "Exam {} is not scheduled yet", exam);
  for (int conflict_exam : conflicts_csrmatrix[exam].indices) {
    int& count = conflicting_exams_count(conflict_exam, current_slot);
    PANIC_IF(count <= 0, "Invalid conflict count for exam {} at slot {}: {}", exam, current_slot,
             count);
    if (--count == 0 && feasible_slots.has_option(conflict_exam, current_slot)) {
      feasible_slots.restore(conflict_exam, current_slot);
    }
  }
  current_slot = -1;
}

}  // namespace common