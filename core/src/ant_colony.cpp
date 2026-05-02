#include <chrono>
#include <cstddef>
#include <random>
#include <omp.h>
#include "ant_colony.hpp"
#include "hyperparameters.hpp"
#include "matrix.hpp"

namespace aco {

AntColony::AntColony(
  int n_exams,
  int n_slots,
  const common::Hyperparams& hp,
  const common::Matrix<int>& s_conflict,
  const std::vector<double>& a_d_slots
) : num_exams(n_exams),
    num_slots(n_slots),
    hyperparams(hp),
    student_conflict(s_conflict),
    absolute_day_slots(a_d_slots),
    conflict_exams(student_conflict),
    total_student_conflict([&]() {
      std::vector<int> total_conflict(num_exams);
      for (size_t exam = 0; exam < num_exams; ++exam) {
        auto [first, last] = conflict_exams.row_range(exam);
        for (auto it = first; it != last; ++it) {
          int neighbor_exam = *it;
          total_conflict[exam] += student_conflict(exam, neighbor_exam);
        }
      }
      return total_conflict;
    }()),
    pheromone(num_exams, num_slots, hyperparams.aco.tau_max),
    ants(hyperparams.aco.num_ants, Ant(num_exams, num_slots)),
    global_best(num_exams, num_slots)
{
  global_best.fitness = std::numeric_limits<double>::infinity();
}

bool AntColony::construct_ant(Ant& ant, std::mt19937& rng) {
  ant.reset();
  std::vector<double> weights(num_slots);
  std::vector<double> delta_soft(num_slots);
  for (size_t i = 0; i < num_exams; ++i) {
    int exam = ant.get_next_exam(total_student_conflict);

    // Check if the solution is infeasible --> ant die now
    int count_feasible = ant.feasible_slots_count[exam];
    if (count_feasible == 0) {
      return false;
    }

    // For each feasible slot, calculate penalty if we assign this exam to that slot
    double total_weight = 0.0;
    for (size_t j = 0; j < count_feasible; ++j) {
      int slot = ant.feasible_slots(exam, j);
      delta_soft[j] = ant.calculate_delta_penalty(exam, slot, student_conflict, conflict_exams, absolute_day_slots);
      double eta = 1.0 / (1.0 + delta_soft[j]);
      weights[j] = std::pow(pheromone(exam, slot), hyperparams.aco.alpha) * std::pow(eta, hyperparams.aco.beta);
      total_weight += weights[j];
    }

    // Choose a random slot using Stochastic Propotional Rule (roulette wheel)
    double threshold = std::uniform_real_distribution<double>(0.0, total_weight)(rng);
    double cumulative = 0.0;
    size_t best_j = count_feasible - 1;
    for (size_t j = 0; j < count_feasible; ++j) {
      cumulative += weights[j];
      if (cumulative >= threshold) {
        best_j = j;
        break;
      }
    }

    int slot = ant.feasible_slots(exam, best_j);
    double penalty = delta_soft[best_j];
    ant.assign_exam(exam, slot, conflict_exams, penalty);
  }
  return true;
}

void AntColony::update_pheromone(const Ant& best_ant) {
  for (size_t exam = 0; exam < num_exams; ++exam) {
    int assigned_slot = best_ant.schedule[exam];
    for (size_t slot = 0; slot < num_slots; ++slot) {
      double delta_tau = hyperparams.aco.rho * hyperparams.aco.tau_min;
      if (slot == assigned_slot) {
        delta_tau = hyperparams.aco.rho * hyperparams.aco.tau_max;
      }
      pheromone(exam, slot) = (1.0 - hyperparams.aco.rho) * pheromone(exam, slot) + delta_tau;
    }
  }
}

void AntColony::run() {
  #pragma omp parallel
  {
    auto time_seed = std::chrono::steady_clock::now().time_since_epoch().count();
    auto thread_id = omp_get_thread_num();
    std::mt19937 thread_rng(time_seed + thread_id);

    #pragma omp for schedule(dynamic)
    for (size_t i = 0; i < hyperparams.aco.num_ants; ++i) {
      bool ok = false;
      while (!ok) {
        ok = construct_ant(ants[i], thread_rng);
      }
      local_search(ants[i], thread_rng);
    }
  }

  int i_best = 0;
  for (size_t i = 1; i < hyperparams.aco.num_ants; ++i) {
    if (ants[i].fitness < ants[i_best].fitness) {
      i_best = i;
    }
  }
  if (ants[i_best].fitness < global_best.fitness) {
    global_best = ants[i_best];
  }
  update_pheromone(ants[i_best]);
}

} // namespace aco