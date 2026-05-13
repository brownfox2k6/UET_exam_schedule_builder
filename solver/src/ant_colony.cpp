#include <cstddef>
#include <pybind11/gil.h>
#include <pybind11/pybind11.h>
#include <omp.h>
#include <random>

#include "aco/ant_colony.hpp"
#include "common/evaluator.hpp"
#include "common/hyperparameters.hpp"
#include "common/matrix.hpp"
#include "common/solution.hpp"

constexpr double HARD_CONSTRAINT_PENALTY = 1e9;

namespace aco {

AntColony::AntColony(
  const common::Hyperparams& hp,
  const common::Matrix<int>& student_conflicts_matrix,
  const std::vector<int64_t>& slot_timestamps,
  int base_seed
) : hyperparams(hp),
    evaluator(hp, slot_timestamps, student_conflicts_matrix),
    num_exams(evaluator.num_exams),
    num_slots(evaluator.num_slots),
    pheromone(num_exams, num_slots, hyperparams.aco.tau_max),
    ants(hyperparams.aco.num_ants, common::Solution(num_exams, num_slots)),
    global_best_schedule(num_exams),
    global_best_fitness(HARD_CONSTRAINT_PENALTY)
{}

bool AntColony::construct_ant(common::Solution& ant) {
  ant.reset();
  const int thread_id = omp_get_thread_num();
  static thread_local std::mt19937 rng(std::random_device{}());
  static thread_local std::vector<double> weights;
  static thread_local std::vector<double> delta_soft;
  weights.resize(num_slots);
  delta_soft.resize(num_slots);

  for (int i = 0; i < num_exams; ++i) {
    const int exam = ant.get_next_exam(evaluator.total_student_conflicts);

    // Check if the solution is infeasible --> ant die now
    const int count_feasible = ant.feasible_slots.get_feasible_count(exam);
    if (count_feasible == 0) {
      ant.fitness = HARD_CONSTRAINT_PENALTY;
      return false;
    }

    // For each feasible slot, calculate penalty if we assign this exam to that slot
    double total_weight = 0.0;
    for (int j = 0; j < count_feasible; ++j) {
      const int slot = ant.feasible_slots(exam, j);
      delta_soft[j] = evaluator.calculate_delta_penalty(ant.schedule, exam, slot);
      const double eta = 1.0 / (1.0 + delta_soft[j]);
      const double tau = pheromone(exam, slot);
      weights[j] = std::pow(tau, hyperparams.aco.alpha) * std::pow(eta, hyperparams.aco.beta);
      total_weight += weights[j];
    }

    // Choose a random slot using Stochastic Propotional Rule (roulette wheel)
    total_weight = std::max(total_weight, 1e-9);
    const double threshold = std::uniform_real_distribution<double>(0.0, total_weight)(rng);
    double cumulative = 0.0;
    int chosen_j = count_feasible - 1;
    for (int j = 0; j < count_feasible; ++j) {
      cumulative += weights[j];
      if (cumulative >= threshold) {
        chosen_j = j;
        break;
      }
    }

    const int slot = ant.feasible_slots(exam, chosen_j);
    const double penalty = delta_soft[chosen_j];
    ant.assign_exam(exam, slot, evaluator.student_conflicts);
    ant.fitness += penalty;
  }
  return true;
}

void AntColony::update_pheromone(const std::vector<int>& best_schedule) {
  for (int exam = 0; exam < num_exams; ++exam) {
    const int assigned_slot = best_schedule[exam];
    for (int slot = 0; slot < num_slots; ++slot) {
      const double delta_tau = slot == assigned_slot
          ? hyperparams.aco.rho * hyperparams.aco.tau_max
          : hyperparams.aco.rho * hyperparams.aco.tau_min;
      pheromone(exam, slot) = (1.0 - hyperparams.aco.rho) * pheromone(exam, slot) + delta_tau;
    }
  }
}

double AntColony::run_one_iteration() {
  #pragma omp parallel for schedule(dynamic)
  for (int i = 0; i < hyperparams.aco.num_ants; ++i) {
    bool ok = false;
    for (int t = 0; !ok && t < 100; ++t) {
      ok = construct_ant(ants[i]);
    }
    if (ok) {
      local_search(ants[i]);
    }
  }

  const common::Solution& iter_best = *std::min_element(ants.begin(), ants.end());
  if (iter_best.fitness < global_best_fitness) {
    global_best_fitness = iter_best.fitness;
    global_best_schedule = iter_best.schedule;
  }
  update_pheromone(iter_best.schedule);
  return iter_best.fitness;
}

void AntColony::run(std::function<void(int, double)> callback) {
  for (int iter = 1; iter <= hyperparams.aco.num_iters; ++iter) {
    const double iter_best_cost = run_one_iteration();
    if (callback) {
      pybind11::gil_scoped_acquire acquire;
      callback(iter, iter_best_cost);
    }
  }
}

} // namespace aco