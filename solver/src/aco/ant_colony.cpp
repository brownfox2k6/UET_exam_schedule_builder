#include "aco/ant_colony.hpp"

#include <omp.h>
#include <pybind11/gil.h>
#include <pybind11/pybind11.h>

#include <cstddef>
#include <cstdint>
#include <utility>

#include "common/room.hpp"

namespace aco {

AntColony::AntColony(common::Hyperparams _hyperparams, const std::vector<common::Exam>& _exams,
                     std::vector<int64_t> _slot_timestamps, const std::vector<common::Room>& _rooms,
                     uint64_t _base_seed)
    : hyperparams(_hyperparams),
      problem_data(_exams, std::move(_slot_timestamps), _rooms),
      evaluator(problem_data, hyperparams),
      base_seed(_base_seed),
      pheromone(problem_data.num_exams, problem_data.num_slots, hyperparams.aco.tau_max()),
      global_best_schedule(static_cast<size_t>(problem_data.num_exams), -1),
      global_best_fitness(HARD_CONSTRAINT_PENALTY) {
  const auto num_ants = static_cast<size_t>(hyperparams.aco.num_ants());
  rngs.reserve(num_ants);
  ants.reserve(num_ants);
  for (size_t i = 0; i < num_ants; ++i) {
    std::seed_seq seq{
        static_cast<uint32_t>(base_seed),
        static_cast<uint32_t>(base_seed >> 32),  // NOLINT(readability-magic-numbers)
        static_cast<uint32_t>(i),                // Differentiates parallel random streams
        0x9e3779b9U  // NOLINT(readability-magic-numbers) Golden-ratio constant for seed mixing
    };
    rngs.emplace_back(seq);
    ants.emplace_back(problem_data);
  }
}

auto AntColony::construct_ant(common::Solution& ant, std::mt19937& rng) -> bool {
  static thread_local std::vector<double> weights;
  static thread_local std::vector<double> delta_soft;
  ant.reset();

  const auto num_exams = static_cast<size_t>(problem_data.num_exams);
  const auto num_slots = static_cast<size_t>(problem_data.num_slots);
  weights.resize(num_slots);
  delta_soft.resize(num_slots);

  for (size_t i = 0; i < num_exams; ++i) {
    const int exam = ant.get_next_exam(problem_data);

    // Check if the solution is infeasible --> ant die now
    const auto count_feasible = static_cast<size_t>(ant.feasible_slots.get_feasible_count(exam));
    if (count_feasible == 0) {
      ant.fitness = HARD_CONSTRAINT_PENALTY;
      return false;
    }

    // For each feasible slot, calculate penalty if we assign this exam to that slot
    double total_weight = 0.0;
    for (size_t j = 0; j < count_feasible; ++j) {
      const int slot = ant.feasible_slots(exam, static_cast<int>(j));
      delta_soft[j] = evaluator.calculate_delta_penalty(ant.assigned_slots, exam, slot);
      const double eta = 1.0 / (1.0 + delta_soft[j]);
      const double tau = pheromone(exam, slot);
      weights[j] = std::pow(tau, hyperparams.aco.alpha()) * std::pow(eta, hyperparams.aco.beta());
      total_weight += weights[j];
    }

    // Choose a random slot using Stochastic Propotional Rule (roulette wheel)
    total_weight = std::max(total_weight, 1e-9);  // NOLINT(readability-magic-numbers)
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

    const int slot = ant.feasible_slots(exam, static_cast<int>(chosen_j));
    const double penalty = delta_soft[chosen_j];
    ant.assign_exam(exam, slot, problem_data.conflicts_csrmatrix);
    ant.fitness += penalty;
  }
  return true;
}

void AntColony::update_pheromone(const std::vector<int>& best_schedule) {
  for (size_t exam = 0; std::cmp_less(exam, problem_data.num_exams); ++exam) {
    const int assigned_slot = best_schedule[exam];
    for (int slot = 0; slot < problem_data.num_slots; ++slot) {
      const double delta_tau = slot == assigned_slot
                                   ? hyperparams.aco.rho() * hyperparams.aco.tau_max()
                                   : hyperparams.aco.rho() * hyperparams.aco.tau_min();
      pheromone(exam, slot) = ((1.0 - hyperparams.aco.rho()) * pheromone(exam, slot)) + delta_tau;
    }
  }
}

auto AntColony::run_one_iteration() -> double {
#pragma omp parallel for schedule(dynamic)
  for (size_t i = 0; i < static_cast<size_t>(hyperparams.aco.num_ants()); ++i) {
    bool is_feasible = false;
    for (int j = 0; !is_feasible && j < hyperparams.aco.max_retries(); ++j) {
      is_feasible = construct_ant(ants[i], rngs[i]);
    }
    if (is_feasible) {
      local_search(ants[i], rngs[i]);
    }
  }

  const common::Solution& iter_best = *std::min_element(ants.begin(), ants.end());
  if (iter_best.fitness >= HARD_CONSTRAINT_PENALTY) {
    return iter_best.fitness;
  }
  if (iter_best.fitness < global_best_fitness) {
    global_best_fitness = iter_best.fitness;
    global_best_schedule = iter_best.assigned_slots;
  }
  update_pheromone(iter_best.assigned_slots);
  return iter_best.fitness;
}

void AntColony::run(const std::function<void(int, double)>& callback) {
  pybind11::gil_scoped_release release;
  for (int iter = 1; iter <= hyperparams.aco.num_iters(); ++iter) {
    const double iter_best_cost = run_one_iteration();
    if (callback) {
      pybind11::gil_scoped_acquire acquire;
      callback(iter, iter_best_cost);
    }
  }
}

}  // namespace aco