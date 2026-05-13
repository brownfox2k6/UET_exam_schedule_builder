#include "common/solution.hpp"
#include "common/matrix.hpp"

namespace common {

Solution::Solution(int n_exams, int n_slots)
  : num_exams(n_exams),
    num_slots(n_slots),
    schedule(num_exams, -1),
    feasible_slots(num_exams, num_slots),
    fitness(0.0),
    conflicting_exams_count(num_exams, num_slots, 0)
{}

bool Solution::operator<(const Solution& other) const {
  return fitness < other.fitness;
}

void Solution::reset() {
  fitness = 0.0;
  feasible_slots.reset();
  std::fill(schedule.begin(), schedule.end(), -1);
  conflicting_exams_count.fill(0);
}

int Solution::get_next_exam(const std::vector<int>& total_student_conflict) const {
  int best_exam = -1;
  int max_degree = -1;
  int max_conflict = -1;
  for (int exam = 0; exam < num_exams; ++exam) {
    if (schedule[exam] != -1) {
      continue;
    }
    int degree = feasible_slots.get_forbidden_count(exam);
    int conflict = total_student_conflict[exam];
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
  const common::CsrMatrix<int>& student_conflicts
) {
  schedule[exam] = slot;
  for (const auto& [conflict_exam, _] : student_conflicts[exam]) {
    if (conflicting_exams_count(conflict_exam, slot) == 0) {
      feasible_slots.remove_option(conflict_exam, slot);
    }
    ++conflicting_exams_count(conflict_exam, slot);
  }
}

void Solution::unassign_exam(
  int exam,
  const common::CsrMatrix<int>& student_conflicts
) {
  int old_slot = schedule[exam];
  assert(old_slot != -1);
  schedule[exam] = -1;
  for (const auto& [conflict_exam, _] : student_conflicts[exam]) {
    int &count = conflicting_exams_count(conflict_exam, old_slot);
    assert(count > 0);
    if (--count == 0) {
      feasible_slots.add_option(conflict_exam, old_slot);
    }
  }
}

} // namespace common