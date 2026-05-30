#include "common/solution.hpp"
#include "utils/assert.hpp"
#include "utils/feasible_set.hpp"
#include "utils/matrix.hpp"

namespace common {

Solution::Solution(const ProblemData& exams): 
  assigned_slots(exams.num_exams, -1),
  feasible_slots(exams.feasible_slots, exams.num_slots),
  feasible_rooms(exams.feasible_rooms, exams.num_rooms),
  fitness(0.0),
  conflicting_exams_count(exams.num_exams, exams.num_slots, 0)
{}

bool Solution::operator<(const Solution& other) const {
  return fitness < other.fitness;
}

void Solution::reset() {
  fitness = 0.0;
  feasible_slots.reset();
  feasible_rooms.reset();
  std::fill(assigned_slots.begin(), assigned_slots.end(), -1);
  conflicting_exams_count.fill(0);
}

int Solution::get_next_exam(const std::vector<int>& total_conflicts) const {
  int best_exam = -1;
  int max_degree = -1;
  int max_conflict = -1;
  for (int exam = 0; exam < assigned_slots.size(); ++exam) {
    if (assigned_slots[exam] != -1) {
      continue;
    }
    int degree = feasible_slots.get_forbidden_count(exam);
    int conflict = total_conflicts[exam];
    if (std::tie(degree, conflict) > std::tie(max_degree, max_conflict)) {
      max_degree = degree;
      max_conflict = conflict;
      best_exam = exam;
    }
  }
  return best_exam;
}

void Solution::assign_exam(
  int exam,
  int slot,
  const utils::CsrMatrix<int>& conflicts_csrmatrix
) {
  PANIC_IF(
    exam < 0 || exam >= assigned_slots.size(),
    "Solution::assign_exam: Exam index {} out of bounds [0, {}]", exam, assigned_slots.size() - 1
  );
  PANIC_IF(
    slot < 0 || slot >= conflicting_exams_count.num_cols(),
    "Solution::assign_exam: Slot index {} out of bounds [0, {}]",
    exam, conflicting_exams_count.num_cols() - 1
  );
  int& current_slot = assigned_slots[exam];
  PANIC_IF(
    current_slot != -1,
    "Solution::assign_exam: Exam {} is already scheduled at slot {}, failed to assign to slot {}",
    exam, current_slot, slot
  );
  PANIC_IF(
    !feasible_slots.is_active(exam, slot),
    "Solution::assign_exam: Exam {} cannot be assigned to inactive/infeasible slot {}",
    exam, slot
  );
  for (int conflict_exam : conflicts_csrmatrix[exam].indices) {
    int& count = conflicting_exams_count(conflict_exam, slot);
    if (count++ == 0 && feasible_slots.has_option(conflict_exam, slot)) {
      feasible_slots.remove(conflict_exam, slot);
    }
  }
  current_slot = slot;
}

void Solution::unassign_exam(
  int exam,
  const utils::CsrMatrix<int>& conflicts_csrmatrix
) {
  PANIC_IF(
    exam < 0 || exam >= assigned_slots.size(),
    "Solution::unassign_exam: Exam index {} out of bounds [0, {}]", exam, assigned_slots.size() - 1
  );
  int& current_slot = assigned_slots[exam];
  PANIC_IF(
    current_slot == -1,
    "Solution::unassign_exam: Exam {} is not scheduled yet", exam
  );
  for (int conflict_exam : conflicts_csrmatrix[exam].indices) {
    int &count = conflicting_exams_count(conflict_exam, current_slot);
    PANIC_IF(
      count <= 0,
      "Solution::unassign_exam: Invalid conflict count for exam {} at slot {}: {}",
      exam, current_slot, count
    );
    if (--count == 0 && feasible_slots.has_option(conflict_exam, current_slot)) {
      feasible_slots.restore(conflict_exam, current_slot);
    }
  }
  current_slot = -1;
}

} // namespace common