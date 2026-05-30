#pragma once

#include "utils/assert.hpp"

#include <cmath>

namespace common {
  
/**
 * @brief A unified container for all algorithmic hyperparameters.
 */
struct Hyperparams {

  /**
   * @brief Parameters related to the problem evaluation and cost function.
   */
  struct Evaluation {
    Evaluation(
      double penalty_decay_base = 2.0
    ) {
      set_penalty_decay_base(penalty_decay_base);
    }

    double penalty_decay_base() const { return penalty_decay_base_; }

    void set_penalty_decay_base(double penalty_decay_base) {
      PANIC_IF(
        !std::isfinite(penalty_decay_base) || penalty_decay_base <= 1.0,
        "Evaluation.penalty_decay_base must be finite and greater than 1.0 for decay behavior (got: {})",
        penalty_decay_base
      );
      penalty_decay_base_ = penalty_decay_base;
    }
  
  private:
    // Base for the exponential decay of time-gap penalties (base ^ -delta_days).
    // Controls how fast the penalty drops: 2.0 halves the penalty each day.
    double penalty_decay_base_;
  }; // struct Evaluation

  /**
   * @brief Hyperparameters controlling the Ant Colony Optimization technique.
   */
  struct ACO {
    ACO(
      int num_iters = 100,
      int num_ants = 10,
      double alpha = 1.0,
      double beta = 2.0,
      double rho = 0.1,
      double tau_min = 0.01,
      double tau_max = 1.0
    ) {
      set_num_iters(num_iters);
      set_num_ants(num_ants);
      set_alpha(alpha);
      set_beta(beta);
      set_rho(rho);
      set_tau_bounds(tau_min, tau_max);
    }

    int num_iters() const { return num_iters_; }
    int num_ants() const { return num_ants_; }
    double alpha() const { return alpha_; }
    double beta() const { return beta_; }
    double rho() const { return rho_; }
    double tau_min() const { return tau_min_; }
    double tau_max() const { return tau_max_; }

    void set_num_iters(int num_iters) {
      PANIC_IF(num_iters <= 0, "ACO.num_iters must be positive (got: {})", num_iters);
      num_iters_ = num_iters;
    }
    void set_num_ants(int num_ants) {
      PANIC_IF(num_ants <= 0, "ACO.num_ants must be positive (got: {})", num_ants);
      num_ants_ = num_ants;
    }
    void set_alpha(double alpha) {
      PANIC_IF(!std::isfinite(alpha) || alpha < 0.0, "ACO.alpha must be finite and non-negative (got: {})", alpha);
      alpha_ = alpha;
    }
    void set_beta(double beta) {
      PANIC_IF(!std::isfinite(beta) || beta < 0.0, "ACO.beta must be finite and non-negative (got: {})", beta);
      beta_ = beta;
    }
    void set_rho(double rho) {
      PANIC_IF(!std::isfinite(rho) || rho < 0.0 || rho > 1.0, "ACO.rho must be finite and in [0, 1] (got: {})", rho);
      rho_ = rho;
    }
    void set_tau_min(double tau_min) {
      PANIC_IF(!std::isfinite(tau_min) || tau_min <= 0.0, "ACO.tau_min must be finite and positive (got: {})", tau_min);
      PANIC_IF(tau_min >= tau_max_, "ACO.tau_min < ACO.tau_max must hold (got: tau_min={}, tau_max={})", tau_min, tau_max_);
      tau_min_ = tau_min;
    }
    void set_tau_max(double tau_max) {
      PANIC_IF(!std::isfinite(tau_max) || tau_max <= 0.0, "ACO.tau_max must be finite and positive (got: {})", tau_max);
      PANIC_IF(tau_min_ >= tau_max, "ACO.tau_min < ACO.tau_max must hold (got: tau_min={}, tau_max={})", tau_min_, tau_max);
      tau_max_ = tau_max;
    }
    void set_tau_bounds(double tau_min, double tau_max) {
      PANIC_IF(!std::isfinite(tau_min) || tau_min <= 0.0, "ACO.tau_min must be finite and positive (got: {})", tau_min);
      PANIC_IF(!std::isfinite(tau_max) || tau_max <= 0.0, "ACO.tau_max must be finite and positive (got: {})", tau_max);
      PANIC_IF(tau_min >= tau_max, "ACO.tau_min < ACO.tau_max must hold (got: tau_min={}, tau_max={})", tau_min, tau_max);
      tau_min_ = tau_min;
      tau_max_ = tau_max;
    }

  private:
    int num_iters_;  // Number of iterations
    int num_ants_;   // Number of ants used in each iteration
    double alpha_;   // Pheromone importance in selection probability (tau ^ alpha)
    double beta_;    // Heuristic importance in selection probability (eta ^ beta)
    double rho_;     // Pheromone evaporation rate for global update
    double tau_min_; // Minimum pheromone trail limit
    double tau_max_; // Maximum pheromone trail limit
  }; // struct ACO

  /**
   * @brief Hyperparameters controlling the Local Search behavior.
   */
  struct LS {
    LS(
      double prob_1_move = 0.5,
      int max_improvements = 10,
      int patience = 100
    ) {
      set_prob_1_move(prob_1_move);
      set_max_improvements(max_improvements);
      set_patience(patience);
    }

    double prob_1_move() const { return prob_1_move_; }
    int max_improvements() const { return max_improvements_; }
    int patience() const { return patience_; }

    void set_prob_1_move(double prob_1_move) {
      PANIC_IF(
        !std::isfinite(prob_1_move) || prob_1_move < 0.0 || prob_1_move > 1.0,
        "LS.prob_1_move must be finite and in [0, 1] (got: {})", prob_1_move
      );
      prob_1_move_ = prob_1_move;
    }
    void set_max_improvements(int max_improvements) {
      PANIC_IF(max_improvements < 0, "LS.max_improvements must be non-negative (got: {})", max_improvements);
      max_improvements_ = max_improvements;
    }
    void set_patience(int patience) {
      PANIC_IF(patience < 0, "LS.patience must be non-negative (got: {})", patience);
      patience_ = patience;
    }
  
  private:
    double prob_1_move_;   // Probability of applying 1-move operator instead of 2-swap
    int max_improvements_; // Maximum number of actual successful moves (downhill steps)
    int patience_;         // Stop early after this many consecutive non-improving attempts
  }; // struct LS

  Evaluation eval;
  ACO aco;
  LS ls;

  Hyperparams(
    Evaluation _eval = Evaluation(),
    ACO _aco = ACO(),
    LS _ls = LS()
  ):
    eval(_eval),
    aco(_aco),
    ls(_ls)
  {}
}; // struct Hyperparams

} // namespace common