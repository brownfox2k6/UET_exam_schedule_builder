#include "slots/aco/ant_colony.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <utility>

#include "evaluation/params.hpp"
#include "evaluation/scoring.hpp"
#include "problem_data.hpp"
#include "slots/aco/params.hpp"
#include "slots/exam_priority.hpp"
#include "slots/slot_assignment.hpp"
#include "slots/slot_result.hpp"
#include "utils/matrix.hpp"
#include "utils/random.hpp"
#include "utils/span.hpp"
#include "utils/vector.hpp"

namespace cetp::slots::aco {
namespace {

// Per-ant mutable state and reusable scratch buffers so OpenMP threads never share or reallocate
// per-slot weight arrays across iterations.
class AntWorker {
public:
  // NOLINTBEGIN(*-non-private-*)
  SlotAssignment assignment;
  Vector<double> slot_weights;
  Vector<double> slot_delta_penalties;
  // NOLINTEND(*-non-private-*)

  AntWorker(const ProblemData& problem, uint64_t base_seed, int ant)
      : assignment(problem),
        slot_weights(problem.num_slots, 0.0),
        slot_delta_penalties(problem.num_slots, 0.0),
        random_engine_(make_random_engine(base_seed, ant)) {}

  // Selects an available slot in proportion to its normalized weight in `slot_weights`.
  [[nodiscard]] auto sample_slot(Span<const int> available_slots, double total_weight) -> int {
    const double threshold = random_double(random_engine_, 0.0, total_weight);
    double cumulative = 0.0;
    for (int slot : available_slots) {
      cumulative += slot_weights[slot];
      if (cumulative > threshold) {
        return slot;
      }
    }
    return available_slots.back();
  }

private:
  std::mt19937 random_engine_;
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
    for (int ant = 0; ant < num_ants; ++ant) {
      workers_.emplace_back(problem_, base_seed, ant);
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
      const double delta = evaluation::calculate_delta_penalty(
          {.problem = problem_,
           .proximity_penalties = proximity_penalties_,
           .schedule = worker.assignment.assigned_slots,
           .exam = exam,
           .new_slot = slot}
      );
      worker.slot_delta_penalties[slot] = delta;

      const double eta = 1.0 / (1.0 + delta);     // heuristic
      const double tau = pheromone_[exam, slot];  // pheromone
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
      const int slot = worker.sample_slot(available_slots, total_weight);
      worker.assignment.assign_exam(exam, slot, problem_.conflicts_csr);
      worker.assignment.penalty += worker.slot_delta_penalties[slot];
    }
    return true;
  }

  void update_pheromone(const Vector<int>& schedule) {
    const double retention = 1.0 - params_.rho();
    for (int exam : problem_.exams()) {
      const int assigned_slot = schedule[exam];
      for (int slot : problem_.feasible_slots_by_exam[exam].values) {
        const double target = slot == assigned_slot ? params_.tau_max() : params_.tau_min();
        const double deposit = params_.rho() * target;
        pheromone_[exam, slot] = (retention * pheromone_[exam, slot]) + deposit;
      }
    }
  }

  void run_one_iteration() {
#pragma omp parallel for schedule(dynamic)
    for (int ant = 0; ant < params_.num_ants(); ++ant) {
      bool is_feasible = false;
      for (int retry = 0; !is_feasible && retry < params_.max_retries(); ++retry) {
        is_feasible = construct_ant(workers_[ant]);
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
    evaluation::Params eval_params,
    uint64_t base_seed,
    const SlotProgressCallback& callback
) -> SlotResult {
  const auto proximity_penalties = evaluation::build_proximity_penalties(problem, eval_params);
  return AntColony(problem, params, proximity_penalties, base_seed).run(callback);
}

}  // namespace cetp::slots::aco
