#pragma once

#include <cstdint>
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

  // Buffers
  std::vector<std::mt19937> workspace_rngs;
  std::vector<std::vector<double>> workspace_weights;
  std::vector<std::vector<double>> workspace_delta_soft;

  /**
   * @brief Compressed representation of the exam conflict graph.
   * * Each row `i` contains a list of `CsrElement` objects, where:
   * - `index`: The ID of exam `j` that has at least one student in common with exam `i`.
   * - `value`: The exact number of students shared between exam `i` and `j`.
   */
  const common::CsrMatrix<int> student_conflicts;

  /**
   * @brief Matrix reprsentation of `student_conflicts`.
   * * `student_conflict(i, j)` tells the number of students taking both exams `i` and `j`
   */
  const common::Matrix<int> student_conflicts_matrix;

  /**
   * @brief Precalculated time-gap penalties for O(1) lookup.
   * `proximity_penalties(i, j)` tells the closeness between slot `i` and `j`.
   */
  const common::Matrix<double> proximity_penalties;

  /**
   * @brief The total weighted conflict degree for each exam.
   * * For each exam `i`, this stores the sum of weights (students) across all its 
   * neighbors in the conflict graph: `sum(student_conflicts[i].value)`.
   * Used as a heuristic to identify "heavy" exams that are harder to schedule.
   */
  const std::vector<int> total_student_conflicts;

  // Pheromone matrix representing learned experience for exam-to-slot assignments
  common::Matrix<double> pheromone;

  // Current population of ants used to construct solutions in each iteration
  std::vector<Ant> ants;

  /**
   * @brief Constructs a complete timetable for a single ant using pheromone and heuristic probabilities.
   * @return True if a feasible schedule is fully constructed, false if it gets stuck.
   */
  bool construct_ant(Ant& ant);

  /**
   * @brief Local search operator: Attempts to move a single exam to a different feasible slot.
   * @return True if the move is successfully applied (downhill or neutral step).
   */
  bool go_1_move(Ant& ant);

  /**
   * @brief Local search operator: Attempts to swap the slots of two assigned exams.
   * @return True if the swap is successfully applied and maintains feasibility.
   */
  bool go_2_swap(Ant& ant);

  /**
   * @brief Applies a series of 1-move and 2-swap operators to refine an ant's fully constructed schedule.
   */
  void local_search(Ant& ant);

  /**
   * @brief Evaporates existing pheromones and deposits new pheromones based on the iteration's best ant.
   */
  void update_pheromone(const Ant& best_ant);

public:
  /**
   * @brief Initializes the Ant Colony algorithm, precomputes conflict structures, and sets up the population.
   */
  AntColony(
    const common::Hyperparams& hp,
    const common::Matrix<int>& student_conflicts_matrix,
    const std::vector<int64_t>& slot_timestamps,
    int base_seed = -1
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