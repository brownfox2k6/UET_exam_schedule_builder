#include <omp.h>

#include "aco/ant_colony.hpp"
#include "common/solution.hpp"

namespace aco {

bool AntColony::go_1_move(common::Solution& ant, std::mt19937 &rng) {
  const int exam = std::uniform_int_distribution<int>(0, exams.num_exams - 1)(rng);

  // Check if the exam cannot be moved to any other slot
  const int count_feasible = ant.feasible_slots.get_feasible_count(exam);
  if (count_feasible == 0) {
    return false;
  }

  // Pick a random feasible slot and ensure it's not the current slot
  int new_slot = ant.feasible_slots.get_random(exam, rng);
  if (new_slot == ant.schedule[exam]) {
    return false;
  }

  // Calculate delta if we assign the exam to this new slot
  double delta = evaluator.calculate_delta_penalty(ant.schedule, exam, new_slot);
  if (delta > 0.0) {
    return false;
  }

  // Accept if cost reduces (or may be unchanged)
  ant.unassign_exam(exam, exams.conflicts_csrmatrix);
  ant.assign_exam(exam, new_slot, exams.conflicts_csrmatrix);
  ant.fitness += delta;
  return true;
}

bool AntColony::go_2_swap(common::Solution& ant, std::mt19937& rng) {
  // Pick two random exams, ensure their assigned slots are different and can be swapped
  const int exam1 = std::uniform_int_distribution<int>(0, exams.num_exams - 1)(rng);
  const int exam2 = std::uniform_int_distribution<int>(0, exams.num_exams - 1)(rng);
  const int slot1 = ant.schedule[exam1];
  const int slot2 = ant.schedule[exam2];
  if (exam1 == exam2 || slot1 == slot2) {
    return false;
  }
  if (!ant.feasible_slots.has_option(exam1, slot2)
      || !ant.feasible_slots.has_option(exam2, slot1)) {
    return false;
  }

  // Check for hard constraint violation if we swap their slots
  int conflicts_in_slot1 = ant.conflicting_exams_count(exam2, slot1);
  int conflicts_in_slot2 = ant.conflicting_exams_count(exam1, slot2);
  if (exams.conflicts_matrix(exam1, exam2) > 0) {
    --conflicts_in_slot1;
    --conflicts_in_slot2;
  }
  if (conflicts_in_slot1 > 0 || conflicts_in_slot2 > 0) {
    return false;
  }

  // Calculate delta if we swap their slots
  double delta = evaluator.calculate_delta_penalty(ant.schedule, exam1, slot2, exam2)
               + evaluator.calculate_delta_penalty(ant.schedule, exam2, slot1, exam1);
  if (delta > 0) {
    return false;
  }

  // Accept if cost reduces (or may be unchanged)
  ant.unassign_exam(exam1, exams.conflicts_csrmatrix);
  ant.unassign_exam(exam2, exams.conflicts_csrmatrix);
  ant.assign_exam(exam1, slot2, exams.conflicts_csrmatrix);
  ant.assign_exam(exam2, slot1, exams.conflicts_csrmatrix);
  ant.fitness += delta;
  return true;
}

void AntColony::local_search(common::Solution& ant, std::mt19937 &rng) {
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