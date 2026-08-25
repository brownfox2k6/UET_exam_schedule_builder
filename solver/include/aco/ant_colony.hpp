#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <random>
#include <vector>

#include "common/evaluator.hpp"
#include "common/hyperparameters.hpp"
#include "common/problem_data.hpp"
#include "common/solution.hpp"
#include "utils/matrix.hpp"

constexpr int BASE_SEED_DEFAULT = 42;

namespace aco {

/*
 * @brief The core engine for the Ant Colony Optimization algorithm.
 */
class AntColony {
 private:
  const common::Hyperparams hyperparams;   // NOLINT(*const-or-ref-data-members*)
  const common::ProblemData problem_data;  // NOLINT(*const-or-ref-data-members*)
  const common::Evaluator evaluator;       // NOLINT(*const-or-ref-data-members*)

  const uint64_t base_seed;  // NOLINT(*const-or-ref-data-members*)
  std::vector<std::mt19937> rngs;

  // Pheromone matrix representing learned experience for exam-to-slot assignments
  utils::Matrix<double> pheromone;

  // Current population of ants used to construct solutions in each iteration
  std::vector<common::Solution> ants;

  /**
   * @brief Constructs a complete timetable for a single ant using pheromone and heuristic
   * probabilities.
   * @return True if a feasible schedule is fully constructed, false if it gets stuck.
   */
  auto construct_ant(common::Solution& ant, std::mt19937& rng) -> bool;

  /**
   * @brief Local search operator: Attempts to move a single exam to a different feasible slot.
   * @return True if the move is successfully applied (downhill or neutral step).
   */
  auto go_1_move(common::Solution& ant, std::mt19937& rng) -> bool;

  /**
   * @brief Local search operator: Attempts to swap the slots of two assigned exams.
   * @return True if the swap is successfully applied and maintains feasibility.
   */
  auto go_2_swap(common::Solution& ant, std::mt19937& rng) -> bool;

  /**
   * @brief Applies a series of 1-move and 2-swap operators to refine an ant's fully constructed
   * schedule.
   */
  void local_search(common::Solution& ant, std::mt19937& rng);

  /**
   * @brief Evaporates existing pheromones and deposits new pheromones based on the iteration's best
   * ant.
   */
  void update_pheromone(const std::vector<int>& best_schedule);
  auto run_one_iteration_impl() -> double;

  std::mutex execution_mutex;

 public:
  /**
   * @brief Initializes the Ant Colony algorithm, precomputes conflict structures, and sets up the
   * population.
   */
  AntColony(common::Hyperparams _hyperparams, const std::vector<common::Exam>& _exams,
            std::vector<int64_t> _slot_timestamps, const std::vector<common::Room>& _rooms,
            uint64_t _base_seed = BASE_SEED_DEFAULT);

  // The best solution found since the start of the algorithm
  std::vector<int> global_best_schedule;  // NOLINT(*non-private-member-variables*)
  double global_best_penalty;             // NOLINT(*non-private-member-variables*)

  /**
   * @brief Executes a single iteration of the Ant Colony Optimization algorithm.
   * This involves constructing solutions for all ants in parallel, applying local search, finding
   * the best ant of the generation, updating the global best solution, and updating the pheromone
   * matrix.
   * @return double The best cost (fitness) found by the ants in this iteration.
   */
  auto run_one_iteration() -> double;

  /**
   * @brief Executes the main loop of the Ant Colony Optimization algorithm.
   * @param callback Optional callback function for real-time progress monitoring (e.g., via
   * Python).
   * - Param 1 (int): The current iteration number.
   * - Param 2 (double): The best cost (penalty) found in the current iteration.
   */
  void run(const std::function<void(int, double)>& callback = nullptr);
};

}  // namespace aco