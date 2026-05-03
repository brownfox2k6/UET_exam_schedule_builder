#include "ant.hpp"
#include "matrix.hpp"

namespace aco {

Ant::Ant(size_t n_exams, size_t n_slots)
  : num_exams(n_exams),
    num_slots(n_slots),
    schedule(num_exams, -1),
    feasible_slots(num_exams, num_slots),
    feasible_slots_count(num_exams, num_slots),
    fitness(0.0),
    conflicting_exams_count(num_exams, num_slots, 0),
    forbidden_slots_count(num_exams, 0) 
{
  reset_feasible_slots();
}

Ant& Ant::operator=(const Ant& other) {
  if (this != &other) {
    this->schedule = other.schedule;
    this->fitness = other.fitness;
  }
  return *this;
}

bool Ant::operator<(const Ant& other) const {
  return fitness < other.fitness;
}

void Ant::reset_feasible_slots() {
  for (size_t exam = 0; exam < num_exams; ++exam) {
    for (size_t slot = 0; slot < num_slots; ++slot) {
      feasible_slots(exam, slot) = slot;
    }
  }
}
  
void Ant::reset() {
  std::fill(schedule.begin(), schedule.end(), -1);
  reset_feasible_slots();
  std::fill(feasible_slots_count.begin(), feasible_slots_count.end(), num_slots);
  fitness = 0.0;
  conflicting_exams_count.fill(0);
  std::fill(forbidden_slots_count.begin(), forbidden_slots_count.end(), 0);
}

int Ant::get_next_exam(const std::vector<int>& total_student_conflict) const {
  int best_exam = -1;
  int max_degree = -1;
  int max_conflict = -1;
  for (size_t exam = 0; exam < num_exams; ++exam) {
    if (schedule[exam] != -1) {
      continue;
    }
    int degree = forbidden_slots_count[exam];
    int conflict = total_student_conflict[exam];
    if (std::tie(degree, conflict) > std::tie(max_degree, max_conflict)) {
      max_degree = degree;
      max_conflict = conflict;
      best_exam = exam;
    }
  }
  return best_exam;
}

void Ant::assign_exam(
  int exam,
  int slot,
  const common::CsrMatrix<int>& student_conflicts
) {
  schedule[exam] = slot;
  for (const auto& [conflict_exam, _] : student_conflicts[exam]) {
    if (conflicting_exams_count(conflict_exam, slot) == 0) {
      ++forbidden_slots_count[conflict_exam];
      int target = -1;
      for (size_t j = 0; j < feasible_slots_count[conflict_exam]; ++j) {
        if (feasible_slots(conflict_exam, j) == slot) {
          target = j;
          break;
        }
      }
      if (target != -1) {
        feasible_slots(conflict_exam, target)
          = feasible_slots(conflict_exam, --feasible_slots_count[conflict_exam]);
        ;
      }
    }
    ++conflicting_exams_count(conflict_exam, slot);
  }
}

void Ant::unassign_exam(
  int exam,
  const common::CsrMatrix<int>& student_conflicts
) {
  int old_slot = schedule[exam];
  schedule[exam] = -1;
  for (const auto& [conflict_exam, _] : student_conflicts[exam]) {
    if (--conflicting_exams_count(conflict_exam, old_slot) == 0) {
      --forbidden_slots_count[conflict_exam];
      feasible_slots(conflict_exam, feasible_slots_count[conflict_exam]++) = old_slot;
    }
  }
}

double Ant::calculate_delta_penalty(
  int exam,
  int new_slot,
  const common::CsrMatrix<int>& student_conflicts,
  const common::Matrix<double>& proximity_penalties,
  int ignore_exam
) const {
  const int cur_slot = schedule[exam];
  double delta = 0.0;
  for (const auto& [conflict_exam, weight] : student_conflicts[exam]) {
    const int conflict_slot = schedule[conflict_exam];
    if (conflict_exam == ignore_exam || conflict_slot == -1) {
      continue;
    }
    delta += weight * proximity_penalties(new_slot, conflict_slot);
    if (cur_slot != -1) {
      delta -= weight * proximity_penalties(cur_slot, conflict_slot);
    }
  }
  return delta;
}

} // namespace aco