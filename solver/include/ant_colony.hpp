#pragma once

#include <functional>
#include <vector>
#include <random>
#include "ant.hpp"
#include "hyperparameters.hpp"
#include "csr_matrix.hpp"
#include "matrix.hpp"

namespace aco {

/*
 * @brief The core engine for the Ant Colony Optimization algorithm.
 */
class AntColony {
private:
  const int num_exams;
  const int num_slots;
  const common::Hyperparams hyperparams;

  // `student_conflict(i, j)` tells the number of students taking both exams `i` and `j`
  const common::Matrix<int> student_conflict;
  
  // Absolute time of each slot in days (used to calculate time-gap penalties)
  const std::vector<double> absolute_day_slots;

  // `conflict_exams[i]` contains all exams `j` where `student_conflict(i, j) > 0`
  const common::CsrMatrix<int> conflict_exams;

  // `total_student_conflict[i]` = sum of `student_conflict(i, j)` for all `j`. 
  // It represents the total aggregate conflict degree of exam `i` relative to all other exams.
  const std::vector<int> total_student_conflict;

  // Pheromone matrix representing learned experience for exam-to-slot assignments
  common::Matrix<double> pheromone;

  // Current population of ants used to construct solutions in each iteration
  std::vector<Ant> ants;

  /**
   * @brief Constructs a complete timetable for a single ant using pheromone and heuristic probabilities.
   * @return True if a feasible schedule is fully constructed, false if it gets stuck.
   */
  bool construct_ant(Ant& ant, std::mt19937& rng);

  /**
   * @brief Local search operator: Attempts to move a single exam to a different feasible slot.
   * @return True if the move is successfully applied (downhill or neutral step).
   */
  bool go_1_move(Ant& ant, std::mt19937& rng);

  /**
   * @brief Local search operator: Attempts to swap the slots of two assigned exams.
   * @return True if the swap is successfully applied and maintains feasibility.
   */
  bool go_2_swap(Ant& ant, std::mt19937& rng);

  /**
   * @brief Applies a series of 1-move and 2-swap operators to refine an ant's fully constructed schedule.
   */
  void local_search(Ant& ant, std::mt19937& rng);

  /**
   * @brief Evaporates existing pheromones and deposits new pheromones based on the iteration's best ant.
   */
  void update_pheromone(const Ant& best_ant);

public:
  /**
   * @brief Initializes the Ant Colony algorithm, precomputes conflict structures, and sets up the population.
   */
  AntColony(
    int n_exams,
    int n_slots,
    const common::Hyperparams& hp,
    const common::Matrix<int>& s_conflict,
    const std::vector<double>& a_d_slots
  );

  // The best solution found since the start of the algorithm
  Ant global_best;

  /**
   * @brief Executes a single iteration of the Ant Colony Optimization algorithm.
   * This involves constructing solutions for all ants in parallel, applying 
   * local search, finding the best ant of the generation, updating the 
   * global best solution, and updating the pheromone matrix.
   * @return double The best cost (fitness) found by the ants in this iteration.
   */
  double run_one_iteration();

  /**
   * @brief Executes the main loop of the Ant Colony Optimization algorithm.
   * @param callback Optional callback function for real-time progress monitoring (e.g., via Python).
   * - Param 1 (int): The current iteration number.
   * - Param 2 (double): The best cost (penalty) found in the current iteration.
   */
  void run(std::function<void(int, double)> callback = nullptr);
};

} // namespace aco