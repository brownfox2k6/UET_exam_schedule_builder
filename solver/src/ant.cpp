#include "ant.hpp"

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
  const common::CsrMatrix<int>& conflict_exams,
  double penalty
) {
  schedule[exam] = slot;
  fitness += penalty;
  auto [first, last] = conflict_exams.row_range(exam);
  for (auto it = first; it != last; ++it) {
    int neighbor_exam = *it;
    if (conflicting_exams_count(neighbor_exam, slot) == 0) {
      ++forbidden_slots_count[neighbor_exam];
      int target = -1;
      for (size_t j = 0; j < feasible_slots_count[neighbor_exam]; ++j) {
        if (feasible_slots(neighbor_exam, j) == slot) {
          target = j;
          break;
        }
      }
      if (target != -1) {
        feasible_slots(neighbor_exam, target)
          = feasible_slots(neighbor_exam, --feasible_slots_count[neighbor_exam]);
        ;
      }
    }
    ++conflicting_exams_count(neighbor_exam, slot);
  }
}

void Ant::unassign_exam(
  int exam,
  const common::CsrMatrix<int>& conflict_exams
) {
  int old_slot = schedule[exam];
  schedule[exam] = -1;
  auto [first, last] = conflict_exams.row_range(exam);
  for (auto it = first; it != last; ++it) {
    int neighbor_exam = *it;
    if (--conflicting_exams_count(neighbor_exam, old_slot) == 0) {
      --forbidden_slots_count[neighbor_exam];
      feasible_slots(neighbor_exam, feasible_slots_count[neighbor_exam]++) = old_slot;
    }
  }
}

double Ant::calculate_delta_penalty(
  int exam,
  int new_slot,
  const common::Matrix<int>& student_conflict,
  const common::CsrMatrix<int>& conflict_exams,
  const std::vector<double>& absolute_day_slots,
  const int ignore_exam
) const {
  double delta = 0.0;
  double d_old = schedule[exam] != -1 ? absolute_day_slots[schedule[exam]] : -1.0;
  double d_new = absolute_day_slots[new_slot];
  auto [first, last] = conflict_exams.row_range(exam);
  for (auto it = first; it != last; ++it) {
    int e = *it;
    int weight = student_conflict(exam, e);
    if (weight == 0 || e == ignore_exam || e == exam || schedule[e] == -1) {
      continue;
    }
    double d_cur = absolute_day_slots[schedule[e]];
    double penalty_old = schedule[exam] != -1 ? weight * std::exp2(-std::fabs(d_old - d_cur)) : 0.0;
    double penalty_new = weight * std::exp2(-std::fabs(d_new - d_cur));
    delta += penalty_new - penalty_old;
  }
  return delta;
}

} // namespace aco