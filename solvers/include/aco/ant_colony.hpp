#pragma once

#include <cmath>
#include <cstdint>
#include <functional>
#include <mutex>
#include <random>

#include "common/evaluator.hpp"
#include "common/hyperparameters.hpp"
#include "common/problem_data.hpp"
#include "common/solution.hpp"
#include "utils/matrix.hpp"
#include "utils/vector.hpp"

constexpr int BASE_SEED_DEFAULT = 42;

namespace aco {

/*
 * @brief The core engine for the Ant Colony Optimization algorithm.
 */
class AntColony {
 public:
  /**
   * @brief Initializes the Ant Colony algorithm, precomputes conflict structures, and sets up the
   * population.
   */
  AntColony(common::Hyperparams _hyperparams, const Vector<common::Exam>& _exams,
            Vector<int64_t> _slot_timestamps, const Vector<common::Room>& _rooms,
            uint64_t _base_seed = BASE_SEED_DEFAULT);

  // The best solution found since the start of the algorithm
  Vector<int> global_best_schedule;      // NOLINT(*non-private-member-variables*)
  double global_best_penalty{INFINITY};  // NOLINT(*non-private-member-variables*)

  /**
   * @brief Executes a single iteration of the Ant Colony Optimization algorithm.
   * This involves constructing solutions for all ants in parallel, finding the best ant of the
   * generation, updating the global best solution, and updating the pheromone matrix.
   * @return double The lowest penalty found by the ants in this iteration.
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

 private:
  const common::Hyperparams hyperparams_;   // NOLINT(*const-or-ref-data-members*)
  const common::ProblemData problem_data_;  // NOLINT(*const-or-ref-data-members*)
  const common::Evaluator evaluator_;       // NOLINT(*const-or-ref-data-members*)
  const uint64_t base_seed_;                // NOLINT(*const-or-ref-data-members*)
  Vector<std::mt19937> rngs_;

  // Pheromone matrix representing learned experience for exam-to-slot assignments
  Matrix<double> pheromone_;

  // Current population of ants used to construct solutions in each iteration
  Vector<common::Solution> ants_;

  std::mutex execution_mutex_;

  /**
   * @brief Constructs a complete timetable for a single ant using pheromone and heuristic
   * probabilities.
   * @return `true` if a feasible schedule is fully constructed, `false` if it gets stuck.
   */
  auto construct_ant(common::Solution& ant, std::mt19937& rng) -> bool;

  /**
   * @brief Evaporates existing pheromones and deposits new pheromones based on the iteration's best
   * ant.
   */
  void update_pheromone(const Vector<int>& best_schedule);

  auto run_one_iteration_impl() -> double;
};

}  // namespace aco
