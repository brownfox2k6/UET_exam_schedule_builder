#include "slots/aco/ant_colony.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <random>
#include <utility>

#include "problem_data.hpp"
#include "slots/aco/params.hpp"
#include "slots/eval_params.hpp"
#include "slots/exam_priority.hpp"
#include "slots/scoring.hpp"
#include "slots/slot_assignment.hpp"
#include "slots/slot_result.hpp"
#include "utils/matrix.hpp"
#include "utils/vector.hpp"

namespace cetp::slots::aco {
namespace {

class AntColony {
public:
  AntColony(const ProblemData& problem, const Params& params, uint64_t base_seed);

  auto run(const Matrix<double>& proximity_penalties, const SlotProgressCallback& callback)
      -> SlotResult;

private:
  std::reference_wrapper<const ProblemData> problem_;
  Params params_;
  Vector<std::mt19937> rngs_;
  Matrix<double> pheromone_;
  Vector<SlotAssignment> ants_;
  Vector<int> best_schedule_;
  double best_penalty_{INFINITY};

  auto construct_ant(
      SlotAssignment& ant,
      std::mt19937& rng,
      const Matrix<double>& proximity_penalties
  ) -> bool;
  void run_one_iteration(const Matrix<double>& proximity_penalties);
  void update_pheromone(const Vector<int>& best_schedule);
};

AntColony::AntColony(const ProblemData& problem, const Params& params, uint64_t base_seed)
    : problem_(problem),
      params_(params),
      pheromone_(problem.num_exams, problem.num_slots, params_.tau_max()) {
  const int num_ants = params_.num_ants();
  rngs_.reserve(num_ants);
  ants_.reserve(num_ants);
  for (int i = 0; i < num_ants; ++i) {
    std::seed_seq seq{
        uint32_t(base_seed),
        uint32_t(base_seed >> 32),  // NOLINT(*-magic-numbers)
        uint32_t(i),                // Differentiates parallel random streams
        0x9e3779b9U                 // NOLINT(*-magic-numbers) Golden-ratio constant for seed mixing
    };
    rngs_.emplace_back(seq);
    ants_.emplace_back(problem);
  }
}

auto AntColony::construct_ant(
    SlotAssignment& ant,
    std::mt19937& rng,
    const Matrix<double>& proximity_penalties
) -> bool {
  static thread_local Vector<double> weights;
  static thread_local Vector<double> delta_soft;
  const ProblemData& problem = problem_.get();
  ant.reset();
  weights.resize(problem.num_slots);
  delta_soft.resize(problem.num_slots);

  for (int i = 0; i < problem.num_exams; ++i) {
    const int exam = select_next_exam(problem, ant);

    const int count_available = ant.available_slots_by_exam.get_available_count(exam);
    if (count_available == 0) {
      ant.penalty = INFINITY;
      return false;
    }

    // Score each available slot in log-space:
    //   log(tau^alpha * eta^beta) = alpha * log(tau) + beta * log(eta)
    auto max_log = double(-INFINITY);
    for (int j = 0; j < count_available; ++j) {
      const int slot = ant.available_slots_by_exam[exam, j];
      delta_soft[j] = calculate_delta_penalty(
          {.problem = problem,
           .proximity_penalties = proximity_penalties,
           .schedule = ant.assigned_slots,
           .exam = exam,
           .new_slot = slot}
      );
      const double eta = 1.0 / (1.0 + delta_soft[j]);
      const double tau = pheromone_[exam, slot];
      // this is actually just log(weights[j])
      weights[j] = (params_.alpha() * std::log(tau)) + (params_.beta() * std::log(eta));
      max_log = std::max(max_log, weights[j]);
    }

    // Shift by max_log (log-sum-exp trick) before exp() to preserve exact selection
    // probabilities while preventing overflow and underflow (1 <= total_weight <= count_available).
    double total_weight = 0.0;
    for (int j = 0; j < count_available; ++j) {
      weights[j] = std::exp(weights[j] - max_log);
      total_weight += weights[j];
    }

    // Choose a slot in proportion to its weight.
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

    const int slot = ant.available_slots_by_exam[exam, chosen_j];
    ant.assign_exam(exam, slot, problem.conflicts_csr);
    ant.penalty += delta_soft[chosen_j];
  }
  return true;
}

void AntColony::update_pheromone(const Vector<int>& best_schedule) {
  const ProblemData& problem = problem_.get();
  for (int exam : problem.exams()) {
    const int assigned_slot = best_schedule[exam];
    for (int slot : problem.slots()) {
      const double target = slot == assigned_slot ? params_.tau_max() : params_.tau_min();
      const double delta_tau = params_.rho() * target;
      pheromone_[exam, slot] = ((1.0 - params_.rho()) * pheromone_[exam, slot]) + delta_tau;
    }
  }
}

void AntColony::run_one_iteration(const Matrix<double>& proximity_penalties) {
#pragma omp parallel for schedule(dynamic)
  for (int i = 0; i < params_.num_ants(); ++i) {
    bool is_feasible = false;
    for (int j = 0; !is_feasible && j < params_.max_retries(); ++j) {
      is_feasible = construct_ant(ants_[i], rngs_[i], proximity_penalties);
    }
  }

  const SlotAssignment& best_ant = *std::ranges::min_element(ants_, {}, &SlotAssignment::penalty);
  if (!std::isfinite(best_ant.penalty)) {
    return;
  }
  if (best_ant.penalty < best_penalty_) {
    best_penalty_ = best_ant.penalty;
    best_schedule_ = best_ant.assigned_slots;
  }
  update_pheromone(best_ant.assigned_slots);
}

auto AntColony::run(const Matrix<double>& proximity_penalties, const SlotProgressCallback& callback)
    -> SlotResult {
  for (int iter = 0; iter < params_.num_iters(); ++iter) {
    run_one_iteration(proximity_penalties);
    if (callback) {
      callback(iter + 1, best_penalty_);
    }
  }
  return {std::move(best_schedule_), best_penalty_};
}

}  // namespace

auto solve(
    const ProblemData& problem,
    Params params,
    EvalParams eval_params,
    uint64_t base_seed,
    const SlotProgressCallback& callback
) -> SlotResult {
  const auto proximity_penalties = build_proximity_penalties(problem, eval_params);
  return AntColony(problem, params, base_seed).run(proximity_penalties, callback);
}

}  // namespace cetp::slots::aco
