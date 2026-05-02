#include "ant_colony.hpp"
#include <random>

namespace aco {

bool AntColony::go_1_move(Ant& ant, std::mt19937& rng) {
  int exam = std::uniform_int_distribution<int>(0, num_exams - 1)(rng);

  // Check if the exam cannot be moved to any other slot
  int count_feasible = ant.feasible_slots_count[exam];
  if (count_feasible == 0) {
    return false;
  }

  // Pick a random feasible slot and ensure it's not the current slot
  int j = std::uniform_int_distribution<int>(0, count_feasible - 1)(rng);
  int new_slot = ant.feasible_slots(exam, j);
  if (new_slot == ant.schedule[exam]) {
    return false;
  }

  // Calculate delta if we assign the exam to this new slot
  double delta = ant.calculate_delta_penalty(exam, new_slot, student_conflict, conflict_exams, absolute_day_slots);
  if (delta > 0.0) {
    return false;
  }

  // Accept if cost reduces (or may be unchanged)
  ant.unassign_exam(exam, conflict_exams);
  ant.assign_exam(exam, new_slot, conflict_exams, delta);
  return true;
}

bool AntColony::go_2_swap(Ant& ant, std::mt19937& rng) {
  // Pick two random exams and ensure their assigned slots are different
  int exam1 = std::uniform_int_distribution<int>(0, num_exams - 1)(rng);
  int exam2 = std::uniform_int_distribution<int>(0, num_exams - 1)(rng);
  int slot1 = ant.schedule[exam1];
  int slot2 = ant.schedule[exam2];
  if (exam1 == exam2 || slot1 == slot2) {
    return false;
  }

  // Check for hard constraint violation if we swap their slots
  int conflicts_in_slot1 = ant.conflicting_exams_count(exam2, slot1);
  int conflicts_in_slot2 = ant.conflicting_exams_count(exam1, slot2);
  if (student_conflict(exam1, exam2) > 0) {
    --conflicts_in_slot1;
    --conflicts_in_slot2;
  }
  if (conflicts_in_slot1 > 0 || conflicts_in_slot2 > 0) {
    return false;
  }

  // Calculate delta if we swap their slots
  double delta = ant.calculate_delta_penalty(exam1, slot2, student_conflict, conflict_exams, absolute_day_slots, exam2)
               + ant.calculate_delta_penalty(exam2, slot1, student_conflict, conflict_exams, absolute_day_slots, exam1);
  if (delta > 0) {
    return false;
  }

  // Accept if cost reduces (or may be unchanged)
  ant.unassign_exam(exam1, conflict_exams);
  ant.unassign_exam(exam2, conflict_exams);
  ant.assign_exam(exam1, slot2, conflict_exams);
  ant.assign_exam(exam2, slot1, conflict_exams, delta);
  return true;
}

void AntColony::local_search(Ant& ant, std::mt19937& rng) {
  int improvements = 0;
  int consecutive_fails = 0;

  while (improvements < hyperparams.ls.max_improvements
         && consecutive_fails < hyperparams.ls.patience) {
    double random = std::uniform_real_distribution<double>(0.0, 1.0)(rng);
    bool ok;
    if (random < hyperparams.ls.prob_1_move) {
      ok = go_1_move(ant, rng);
    } else {
      ok = go_2_swap(ant, rng);
    }

    if (ok) {
      ++improvements;
      consecutive_fails = 0;
    } else {
      ++consecutive_fails;
    }
  }
}

} // namespace aco