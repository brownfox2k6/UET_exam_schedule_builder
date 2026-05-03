#include <algorithm>
#include <cstddef>
#include <functional>
#include <numeric>
#include <pybind11/gil.h>
#include <random>
#include <omp.h>
#include <pybind11/pybind11.h>
#include "ant_colony.hpp"
#include "hyperparameters.hpp"
#include "matrix.hpp"

constexpr double SECONDS_PER_DAY = 86400.0;
constexpr double HARD_CONSTRAINT_PENALTY = 1e9;

namespace aco {

AntColony::AntColony(
  int n_exams,
  int n_slots,
  const common::Hyperparams& hp,
  const common::Matrix<int>& student_conflicts_matrix,
  const std::vector<int64_t>& slot_timestamps,
  int base_seed
) : num_exams(n_exams),
    num_slots(n_slots),
    hyperparams(hp),
    student_conflicts(student_conflicts_matrix),
    student_conflicts_matrix(student_conflicts_matrix),
    proximity_penalties([&]() {
      common::Matrix<double> penalties(num_slots, num_slots, 0.0);
      for (size_t i = 0; i < num_slots; ++i) {
        penalties(i, i) = HARD_CONSTRAINT_PENALTY;
        const int64_t slot_i = slot_timestamps[i];
        for (size_t j = i + 1; j < num_slots; ++j) {
          const double diff_days = std::abs(slot_i - slot_timestamps[j]) / SECONDS_PER_DAY;
          const double value = std::pow(hyperparams.aco.penalty_decay_base, -diff_days);
          penalties(i, j) = penalties(j, i) = value;
        }
      }
      return penalties;
    }()),
    total_student_conflicts([&]() {
      std::vector<int> totals(num_exams);
      for (size_t exam = 0; exam < num_exams; ++exam) {
        const auto row = student_conflicts[exam];
        totals[exam] = std::accumulate(row.begin(), row.end(), 0);
      }
      return totals;
    }()),
    pheromone(num_exams, num_slots, hyperparams.aco.tau_max),
    ants(hyperparams.aco.num_ants, Ant(num_exams, num_slots)),
    global_best(num_exams, num_slots)
{
  global_best.fitness = std::numeric_limits<double>::infinity();

  const int max_threads = omp_get_max_threads();
  std::random_device rd;
  for (int i = 0; i < max_threads; ++i) {
    workspace_rngs.emplace_back(base_seed == -1 ? rd() : base_seed + i);
  }
  workspace_weights.resize(num_slots);
  workspace_delta_soft.resize(num_slots);
}

bool AntColony::construct_ant(Ant& ant) {
  ant.reset();
  const int thread_id = omp_get_thread_num();
  std::mt19937& rng = workspace_rngs[thread_id];
  std::vector<double>& weights = workspace_weights[thread_id];
  std::vector<double>& delta_soft = workspace_delta_soft[thread_id];

  for (size_t i = 0; i < num_exams; ++i) {
    const int exam = ant.get_next_exam(total_student_conflicts);

    // Check if the solution is infeasible --> ant die now
    const int count_feasible = ant.feasible_slots_count[exam];
    if (count_feasible == 0) {
      return false;
    }

    // For each feasible slot, calculate penalty if we assign this exam to that slot
    double total_weight = 0.0;
    for (size_t j = 0; j < count_feasible; ++j) {
      const int slot = ant.feasible_slots(exam, j);
      delta_soft[j] = ant.calculate_delta_penalty(exam, slot, student_conflicts, proximity_penalties);
      const double eta = 1.0 / (1.0 + delta_soft[j]);
      const double tau = pheromone(exam, slot);
      weights[j] = std::pow(tau, hyperparams.aco.alpha) * std::pow(eta, hyperparams.aco.beta);
      total_weight += weights[j];
    }

    // Choose a random slot using Stochastic Propotional Rule (roulette wheel)
    const double threshold = std::uniform_real_distribution<double>(0.0, total_weight)(rng);
    double cumulative = 0.0;
    size_t chosen_j = count_feasible - 1;
    for (size_t j = 0; j < count_feasible; ++j) {
      cumulative += weights[j];
      if (cumulative >= threshold) {
        chosen_j = j;
        break;
      }
    }

    const int slot = ant.feasible_slots(exam, chosen_j);
    const double penalty = delta_soft[chosen_j];
    ant.assign_exam(exam, slot, student_conflicts);
    ant.fitness += penalty;
  }
  return true;
}

void AntColony::update_pheromone(const Ant& best_ant) {
  for (size_t exam = 0; exam < num_exams; ++exam) {
    const int assigned_slot = best_ant.schedule[exam];
    for (size_t slot = 0; slot < num_slots; ++slot) {
      const double delta_tau = slot == assigned_slot
          ? hyperparams.aco.rho * hyperparams.aco.tau_max
          : hyperparams.aco.rho * hyperparams.aco.tau_min;
      pheromone(exam, slot) = (1.0 - hyperparams.aco.rho) * pheromone(exam, slot) + delta_tau;
    }
  }
}

double AntColony::run_one_iteration() {
  #pragma omp parallel for schedule(dynamic)
  for (size_t i = 0; i < hyperparams.aco.num_ants; ++i) {
    bool ok = false;
    while (!ok) {
      ok = construct_ant(ants[i]);
    }
    local_search(ants[i]);
  }

  const Ant& iter_best = *std::min_element(ants.begin(), ants.end());
  if (iter_best.fitness < global_best.fitness) {
    global_best = iter_best;
  }
  update_pheromone(iter_best);
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