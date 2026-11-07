#include "aco/ant_colony.hpp"

#include <omp.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "common/room.hpp"
#include "common/slot_assignment.hpp"
#include "common/solution.hpp"

namespace aco {

AntColony::AntColony(common::Hyperparams _hyperparams, const Vector<common::Exam>& _exams,
                     Vector<int64_t> _slot_timestamps, const Vector<common::Room>& _rooms,
                     uint64_t _base_seed)
    : global_best_schedule(_exams.size(), -1),
      hyperparams_(_hyperparams),
      problem_data_(_exams, std::move(_slot_timestamps), _rooms),
      evaluator_(problem_data_, hyperparams_.eval),
      base_seed_(_base_seed),
      pheromone_(problem_data_.num_exams, problem_data_.num_slots, hyperparams_.aco.tau_max()) {
  const int num_ants = hyperparams_.aco.num_ants();
  rngs_.reserve(num_ants);
  ants_.reserve(num_ants);
  for (int i = 0; i < num_ants; ++i) {
    std::seed_seq seq{
        uint32_t(base_seed_),
        uint32_t(base_seed_ >> 32),  // NOLINT(*-magic-numbers)
        uint32_t(i),                 // Differentiates parallel random streams
        0x9e3779b9U  // NOLINT(*-magic-numbers) Golden-ratio constant for seed mixing
    };
    rngs_.emplace_back(seq);
    ants_.emplace_back(problem_data_);
  }
}

auto AntColony::construct_ant(common::Solution& ant, std::mt19937& rng) -> bool {
  static thread_local Vector<double> weights;
  static thread_local Vector<double> delta_soft;
  ant.reset();
  common::SlotAssignment& slot_assignment = ant.slot_assignment;

  const int num_exams = problem_data_.num_exams;
  const int num_slots = problem_data_.num_slots;
  weights.resize(num_slots);
  delta_soft.resize(num_slots);

  for (int i = 0; i < num_exams; ++i) {
    const int exam = slot_assignment.get_next_exam(problem_data_);

    // Check if the solution is infeasible --> ant die now
    const int count_available = slot_assignment.available_slots_by_exam.get_available_count(exam);
    if (count_available == 0) {
      ant.penalty = INFINITY;
      return false;
    }

    // For each available slot, calculate penalty if we assign this exam to that slot
    double total_weight = 0.0;
    for (int j = 0; j < count_available; ++j) {
      const int slot = slot_assignment.available_slots_by_exam[exam, j];
      delta_soft[j] =
          evaluator_.calculate_delta_penalty(slot_assignment.assigned_slots, exam, slot);
      const double eta = 1.0 / (1.0 + delta_soft[j]);
      const double tau = pheromone_[exam, slot];
      weights[j] = std::pow(tau, hyperparams_.aco.alpha()) * std::pow(eta, hyperparams_.aco.beta());
      total_weight += weights[j];
    }

    // Choose a random slot using Stochastic Propotional Rule (roulette wheel)
    total_weight = std::max(total_weight, 1e-9);  // NOLINT(*-magic-numbers)
    const double threshold = std::uniform_real_distribution<double>(0.0, total_weight)(rng);
    double cumulative = 0.0;
    int chosen_j = count_available - 1;
    for (int j = 0; j < count_available; ++j) {
      cumulative += weights[j];
      if (cumulative >= threshold) {
        chosen_j = j;
        break;
      }
    }

    const int slot = slot_assignment.available_slots_by_exam[exam, chosen_j];
    const double penalty = delta_soft[chosen_j];
    slot_assignment.assign_exam(exam, slot, problem_data_.conflicts_csrmatrix);
    ant.penalty += penalty;
  }
  return true;
}

void AntColony::update_pheromone(const Vector<int>& best_schedule) {
  for (int exam = 0; exam < problem_data_.num_exams; ++exam) {
    const int assigned_slot = best_schedule[exam];
    for (int slot = 0; slot < problem_data_.num_slots; ++slot) {
      const double delta_tau = slot == assigned_slot
                                   ? hyperparams_.aco.rho() * hyperparams_.aco.tau_max()
                                   : hyperparams_.aco.rho() * hyperparams_.aco.tau_min();
      pheromone_[exam, slot] =
          ((1.0 - hyperparams_.aco.rho()) * pheromone_[exam, slot]) + delta_tau;
    }
  }
}

auto AntColony::run_one_iteration_impl() -> double {
#pragma omp parallel for schedule(dynamic)
  for (int i = 0; i < hyperparams_.aco.num_ants(); ++i) {
    const int ant_index = i;
    bool is_feasible = false;
    for (int j = 0; !is_feasible && j < hyperparams_.aco.max_retries(); ++j) {
      is_feasible = construct_ant(ants_[ant_index], rngs_[ant_index]);
    }
  }

  const common::Solution& iter_best =
      *std::ranges::min_element(ants_, {}, &common::Solution::penalty);
  const double iter_best_penalty = iter_best.penalty;
  const Vector<int>& iter_best_schedule = iter_best.slot_assignment.assigned_slots;

  if (std::isinf(iter_best_penalty)) {
    return INFINITY;
  }
  if (iter_best_penalty < global_best_penalty) {
    global_best_penalty = iter_best_penalty;
    global_best_schedule = iter_best_schedule;
  }
  update_pheromone(iter_best_schedule);
  return iter_best_penalty;
}

auto AntColony::run_one_iteration() -> double {
  const std::scoped_lock lock(execution_mutex_);
  return run_one_iteration_impl();
}

void AntColony::run(const std::function<void(int, double)>& callback) {
  const std::scoped_lock lock(execution_mutex_);
  for (int iter = 1; iter <= hyperparams_.aco.num_iters(); ++iter) {
    const double iter_best_cost = run_one_iteration_impl();
    if (callback) {
      callback(iter, iter_best_cost);
    }
  }
}

}  // namespace aco
