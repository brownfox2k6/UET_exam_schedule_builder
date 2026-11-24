#include "slots/slot_assignment.hpp"

#include <algorithm>
#include <cassert>

#include "problem_data.hpp"
#include "utils/available_set.hpp"
#include "utils/csr_matrix.hpp"
#include "utils/matrix.hpp"
#include "utils/vector.hpp"

namespace cetp::slots {

SlotAssignment::SlotAssignment(const ProblemData& problem)
    : conflicting_exams_count(problem.num_exams, problem.num_slots, 0),
      assigned_slots(problem.num_exams, -1),
      available_slots_by_exam(problem.feasible_slots_by_exam, problem.num_slots) {}

void SlotAssignment::assign_exam(int exam, int slot, const CsrMatrix<int>& conflicts_csr) {
  assert(exam >= 0 && exam < assigned_slots.size());
  assert(slot >= 0 && slot < conflicting_exams_count.num_cols());
  int& current_slot = assigned_slots[exam];
  assert(current_slot == -1 && "Exam must be unassigned");
  assert(available_slots_by_exam.is_available(exam, slot));

  for (int conflict_exam : conflicts_csr[exam].indices) {
    int& count = conflicting_exams_count[conflict_exam, slot];
    if (count++ == 0 && available_slots_by_exam.has_option(conflict_exam, slot)) {
      available_slots_by_exam.remove(conflict_exam, slot);
    }
  }
  current_slot = slot;
}

void SlotAssignment::unassign_exam(int exam, const CsrMatrix<int>& conflicts_csr) {
  assert(exam >= 0 && exam < assigned_slots.size());
  int& current_slot = assigned_slots[exam];
  assert(current_slot >= 0 && current_slot < conflicting_exams_count.num_cols());

  for (int conflict_exam : conflicts_csr[exam].indices) {
    int& count = conflicting_exams_count[conflict_exam, current_slot];
    assert(count > 0);
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
  penalty = 0.0;
}

}  // namespace cetp::slots
