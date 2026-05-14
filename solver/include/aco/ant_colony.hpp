#pragma once

#include <cstdint>
#include <functional>
#include <random>
#include <vector>

#include "../common/solution.hpp"
#include "../common/evaluator.hpp"
#include "../common/hyperparameters.hpp"
#include "../common/matrix.hpp"

namespace aco {

/*
 * @brief The core engine for the Ant Colony Optimization algorithm.
 */
class AntColony {
private:
  const common::Hyperparams hyperparams;
  const common::Evaluator evaluator;
  const uint64_t base_seed;

  const int num_exams;
  const int num_slots;

  std::vector<std::mt19937> rngs;

  // Pheromone matrix representing learned experience for exam-to-slot assignments
  common::Matrix<double> pheromone;

  // Current population of ants used to construct solutions in each iteration
  std::vector<common::Solution> ants;

  /**
   * @brief Constructs a complete timetable for a single ant using pheromone and heuristic probabilities.
   * @return True if a feasible schedule is fully constructed, false if it gets stuck.
   */
  bool construct_ant(common::Solution& ant, std::mt19937& rng);

  /**
   * @brief Local search operator: Attempts to move a single exam to a different feasible slot.
   * @return True if the move is successfully applied (downhill or neutral step).
   */
  bool go_1_move(common::Solution& ant, std::mt19937& rng);

  /**
   * @brief Local search operator: Attempts to swap the slots of two assigned exams.
   * @return True if the swap is successfully applied and maintains feasibility.
   */
  bool go_2_swap(common::Solution& ant, std::mt19937& rng);

  /**
   * @brief Applies a series of 1-move and 2-swap operators to refine an ant's fully constructed schedule.
   */
  void local_search(common::Solution& ant, std::mt19937& rng);

  /**
   * @brief Evaporates existing pheromones and deposits new pheromones based on the iteration's best ant.
   */
  void update_pheromone(const std::vector<int>& best_schedule);

public:
  /**
   * @brief Initializes the Ant Colony algorithm, precomputes conflict structures, and sets up the population.
   */
  AntColony(
    const common::Hyperparams& hp,
    const common::Matrix<int>& student_conflicts_matrix,
    const std::vector<int64_t>& slot_timestamps,
    int64_t _base_seed = -1
  );

  // The best solution found since the start of the algorithm
  std::vector<int> global_best_schedule;
  double global_best_fitness;

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