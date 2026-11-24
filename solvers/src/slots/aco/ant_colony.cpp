#include "slots/aco/ant_colony.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
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
#include "utils/span.hpp"
#include "utils/vector.hpp"

namespace cetp::slots::aco {
namespace {

constexpr uint32_t HIGH_BITS_SHIFT = 32;
// Golden-ratio constant mixed into each ant's seed sequence to differentiate parallel streams.
constexpr uint32_t SEED_MIX_CONSTANT = 0x9e3779b9;

// Per-ant mutable state and reusable scratch buffers so OpenMP threads never share or reallocate
// per-slot weight arrays across iterations.
class AntWorker {
public:
  // NOLINTBEGIN(*-non-private-*)
  SlotAssignment assignment;
  Vector<double> slot_weights;
  Vector<double> slot_delta_penalties;
  // NOLINTEND(*-non-private-*)

  AntWorker(const ProblemData& problem, uint64_t base_seed, int ant_index)
      : assignment(problem),
        slot_weights(problem.num_slots, 0.0),
        slot_delta_penalties(problem.num_slots, 0.0),
        random_engine_(make_random_engine(base_seed, ant_index)) {}

  // Selects an available slot in proportion to its normalized weight in `slot_weights`.
  [[nodiscard]] auto sample_slot(Span<const int> available_slots, double total_weight) -> int {
    std::uniform_real_distribution<double> weight_distribution(0.0, total_weight);
    const double threshold = weight_distribution(random_engine_);
    double cumulative_weight = 0.0;
    for (int slot : available_slots) {
      cumulative_weight += slot_weights[slot];
      if (cumulative_weight > threshold) {
        return slot;
      }
    }
    return available_slots.back();
  }

private:
  std::mt19937 random_engine_;

  [[nodiscard]] static auto make_random_engine(uint64_t base_seed, int ant_index) -> std::mt19937 {
    std::seed_seq seed_sequence{
        uint32_t(base_seed),
        uint32_t(base_seed >> HIGH_BITS_SHIFT),
        uint32_t(ant_index),
        SEED_MIX_CONSTANT
    };
    return std::mt19937(seed_sequence);
  }
};

class AntColony {
public:
  AntColony(
      const ProblemData& problem,
      const Params& params,
      const Matrix<double>& proximity_penalties,
      uint64_t base_seed
  )
      : problem_(problem),
        proximity_penalties_(proximity_penalties),
        params_(params),
        pheromone_(problem.num_exams, problem.num_slots, params_.tau_max()) {
    const int num_ants = params_.num_ants();
    workers_.reserve(num_ants);
    for (int ant_index = 0; ant_index < num_ants; ++ant_index) {
      workers_.emplace_back(problem_, base_seed, ant_index);
    }
  }

  [[nodiscard]] auto run(const SlotProgressCallback& callback) -> SlotResult {
    for (int iteration = 0; iteration < params_.num_iters(); ++iteration) {
      run_one_iteration();
      if (callback) {
        callback(iteration + 1, best_penalty_);
      }
    }
    return {.schedule = std::move(best_schedule_), .penalty = best_penalty_};
  }

private:
  // NOLINTBEGIN(cppcoreguidelines-avoid-const-or-ref-data-members)
  const ProblemData& problem_;
  const Matrix<double>& proximity_penalties_;
  // NOLINTEND(cppcoreguidelines-avoid-const-or-ref-data-members)
  Params params_;
  Matrix<double> pheromone_;
  Vector<AntWorker> workers_;
  Vector<int> best_schedule_;
  double best_penalty_ = INFINITY;

  // Scores each available slot in log-space (`alpha * log(tau) + beta * log(eta)`) and applies the
  // log-sum-exp shift before `exp()` to preserve exact probabilities without overflow or underflow.
  [[nodiscard]] auto score_available_slots(
      AntWorker& worker,
      int exam,
      Span<const int> available_slots
  ) const -> double {
    auto max_log_weight = double(-INFINITY);
    for (int slot : available_slots) {
      const double delta_penalty = calculate_delta_penalty(
          {.problem = problem_,
           .proximity_penalties = proximity_penalties_,
           .schedule = worker.assignment.assigned_slots,
           .exam = exam,
           .new_slot = slot}
      );
      worker.slot_delta_penalties[slot] = delta_penalty;

      const double eta = 1.0 / (1.0 + delta_penalty);  // heuristic
      const double tau = pheromone_[exam, slot];       // pheromone
      const double log_weight =
          (params_.alpha() * std::log(tau)) + (params_.beta() * std::log(eta));
      worker.slot_weights[slot] = log_weight;
      max_log_weight = std::max(max_log_weight, log_weight);
    }

    double total_weight = 0.0;
    for (int slot : available_slots) {
      worker.slot_weights[slot] = std::exp(worker.slot_weights[slot] - max_log_weight);
      total_weight += worker.slot_weights[slot];
    }
    return total_weight;
  }

  [[nodiscard]] auto construct_ant(AntWorker& worker) const -> bool {
    worker.assignment.reset();
    for (int step = 0; step < problem_.num_exams; ++step) {
      const int exam = select_next_exam(problem_, worker.assignment);
      const Span<const int> available_slots = worker.assignment.available_slots_by_exam[exam];
      if (available_slots.empty()) {
        worker.assignment.penalty = INFINITY;
        return false;
      }

      const double total_weight = score_available_slots(worker, exam, available_slots);
      const int chosen_slot = worker.sample_slot(available_slots, total_weight);
      worker.assignment.assign_exam(exam, chosen_slot, problem_.conflicts_csr);
      worker.assignment.penalty += worker.slot_delta_penalties[chosen_slot];
    }
    return true;
  }

  void update_pheromone(const Vector<int>& iteration_best_schedule) {
    const double evaporation_factor = 1.0 - params_.rho();
    for (int exam : problem_.exams()) {
      const int assigned_slot = iteration_best_schedule[exam];
      for (int slot : problem_.feasible_slots_by_exam[exam].values) {
        const double target_pheromone =
            slot == assigned_slot ? params_.tau_max() : params_.tau_min();
        const double pheromone_deposit = params_.rho() * target_pheromone;
        pheromone_[exam, slot] = (evaporation_factor * pheromone_[exam, slot]) + pheromone_deposit;
      }
    }
  }

  void run_one_iteration() {
#pragma omp parallel for schedule(dynamic)
    for (int ant_index = 0; ant_index < params_.num_ants(); ++ant_index) {
      bool is_feasible = false;
      for (int retry_index = 0; !is_feasible && retry_index < params_.max_retries();
           ++retry_index) {
        is_feasible = construct_ant(workers_[ant_index]);
      }
    }

    const AntWorker& best_worker =
        *std::ranges::min_element(workers_, {}, [](const AntWorker& worker) -> double {
          return worker.assignment.penalty;
        });
    if (!std::isfinite(best_worker.assignment.penalty)) {
      return;
    }
    if (best_worker.assignment.penalty < best_penalty_) {
      best_penalty_ = best_worker.assignment.penalty;
      best_schedule_ = best_worker.assignment.assigned_slots;
    }
    update_pheromone(best_worker.assignment.assigned_slots);
  }
};

}  // namespace

auto solve(
    const ProblemData& problem,
    Params params,
    EvalParams eval_params,
    uint64_t base_seed,
    const SlotProgressCallback& callback
) -> SlotResult {
  const auto proximity_penalties = build_proximity_penalties(problem, eval_params);
  return AntColony(problem, params, proximity_penalties, base_seed).run(callback);
}

}  // namespace cetp::slots::aco
