#pragma once

#include <vector>
#include <random>
#include "structures.hpp"
#include "hyperparameters.hpp"

namespace aco {

/*
 * @brief The core engine for the Ant Colony Optimization technique.
 */
class AntColony {
private:
  const int num_exams;
  const int num_slots;
  const common::Hyperparams hyperparams;

  // `student_conflict(i, j)` tells the number of students taking both exams `i` and `j`
  const common::Matrix<int>& student_conflict;
  
  // Absolute time of each slot in days (used to calculate time-gap penalties)
  const std::vector<double>& absolute_day_slots;

  // `conflict_exams[i]` contains all exams `j` where `student_conflict(i, j) > 0`
  const common::CsrMatrix<int> conflict_exams;

  // `total_student_conflict[i]` = sum of `student_conflict(i, j)` for all `j`. 
  // It represents the total aggregate conflict degree of exam `i` relative to all other exams.
  const std::vector<int> total_student_conflict;

  // Pheromone matrix representing learned experience for exam-to-slot assignments
  common::Matrix<double> pheromone;

  // Current population of ants used to construct solutions in each iteration
  std::vector<Ant> ants;

  int get_next_exam(const Ant& ant, const std::vector<int>& forbidden_count) const;
  bool construct_ant(Ant& ant, std::mt19937& rng);

  bool go_1_move(Ant& ant, std::mt19937& rng);
  bool go_2_swap(Ant& ant, std::mt19937& rng);
  void local_search(Ant& ant, std::mt19937& rng);

  void update_pheromone(const Ant& best_ant);

public:
  AntColony(
    int n_exams,
    int n_slots,
    const common::Hyperparams& hp,
    const common::Matrix<int>& s_conflict,
    const std::vector<double>& a_d_slots
  );

  // The best solution found since the start of the algorithm
  Ant global_best;

  void run();
};

} // namespace aco