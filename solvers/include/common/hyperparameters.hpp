#pragma once

#include <cmath>
#include <format>
#include <stdexcept>

namespace common {

/**
 * @brief A unified container for all algorithmic hyperparameters.
 */
struct Hyperparams {
  /**
   * @brief Parameters related to the problem evaluation and cost function.
   */
  struct Evaluation {
    Evaluation();

    [[nodiscard]] auto penalty_decay_base() const noexcept -> double { return penalty_decay_base_; }

    void set_penalty_decay_base(double penalty_decay_base) {
      if (!std::isfinite(penalty_decay_base) || penalty_decay_base <= 1.0) {
        throw std::invalid_argument(std::format(
            "Evaluation.penalty_decay_base must be finite and greater than 1.0 (got: {})",
            penalty_decay_base));
      }
      penalty_decay_base_ = penalty_decay_base;
    }

   private:
    // NOLINTBEGIN(*-magic-numbers)

    // Base for the exponential decay of time-gap penalties (base ^ -delta_days).
    // Controls how fast the penalty drops: 2.0 halves the penalty each day.
    double penalty_decay_base_{2.0};
    // NOLINTEND(*-magic-numbers)
  };  // struct Evaluation

  /**
   * @brief Hyperparameters controlling the Ant Colony Optimization technique.
   */
  struct ACO {
    ACO();

    [[nodiscard]] auto num_iters() const noexcept -> int { return num_iters_; }
    [[nodiscard]] auto num_ants() const noexcept -> int { return num_ants_; }
    [[nodiscard]] auto alpha() const noexcept -> double { return alpha_; }
    [[nodiscard]] auto beta() const noexcept -> double { return beta_; }
    [[nodiscard]] auto rho() const noexcept -> double { return rho_; }
    [[nodiscard]] auto tau_min() const noexcept -> double { return tau_min_; }
    [[nodiscard]] auto tau_max() const noexcept -> double { return tau_max_; }
    [[nodiscard]] auto max_retries() const noexcept -> int { return max_retries_; }

    void set_num_iters(int num_iters) {
      if (num_iters <= 0) {
        throw std::invalid_argument(
            std::format("ACO.num_iters must be positive (got: {})", num_iters));
      }
      num_iters_ = num_iters;
    }

    void set_num_ants(int num_ants) {
      if (num_ants <= 0) {
        throw std::invalid_argument(
            std::format("ACO.num_ants must be positive (got: {})", num_ants));
      }
      num_ants_ = num_ants;
    }

    void set_alpha(double alpha) {
      if (!std::isfinite(alpha) || alpha < 0.0) {
        throw std::invalid_argument(
            std::format("ACO.alpha must be finite and non-negative (got: {})", alpha));
      }
      alpha_ = alpha;
    }

    void set_beta(double beta) {
      if (!std::isfinite(beta) || beta < 0.0) {
        throw std::invalid_argument(
            std::format("ACO.beta must be finite and non-negative (got: {})", beta));
      }
      beta_ = beta;
    }

    void set_rho(double rho) {
      if (!std::isfinite(rho) || rho < 0.0 || rho > 1.0) {
        throw std::invalid_argument(
            std::format("ACO.rho must be finite and in [0, 1] (got: {})", rho));
      }
      rho_ = rho;
    }

    void set_tau_min(double tau_min) {
      if (!std::isfinite(tau_min) || tau_min <= 0.0 || tau_min >= tau_max_) {
        throw std::invalid_argument(std::format(
            "ACO.tau_min must be finite, positive, and below tau_max (got: {}, tau_max: {})",
            tau_min, tau_max_));
      }
      tau_min_ = tau_min;
    }

    void set_tau_max(double tau_max) {
      if (!std::isfinite(tau_max) || tau_max <= tau_min_) {
        throw std::invalid_argument(std::format(
            "ACO.tau_max must be finite, positive, and above tau_min (got: {}, tau_min: {})",
            tau_max, tau_min_));
      }
      tau_max_ = tau_max;
    }

    void set_tau_bounds(double tau_min, double tau_max) {
      if (!std::isfinite(tau_max) || !std::isfinite(tau_min) || tau_min <= 0.0 ||
          tau_min >= tau_max) {
        throw std::invalid_argument(std::format(
            "ACO bounds satisfy 0 < tau_min < tau_max < inf (got: {}, {})", tau_min, tau_max));
      }
      tau_min_ = tau_min;
      tau_max_ = tau_max;
    }

    void set_max_retries(int max_retries) {
      if (max_retries <= 0) {
        throw std::invalid_argument(
            std::format("ACO.max_retries must be positive (got: {})", max_retries));
      }
      max_retries_ = max_retries;
    }

   private:
    // NOLINTBEGIN(*-magic-numbers)
    int num_iters_{100};    // Number of iterations
    int num_ants_{10};      // Number of ants used in each iteration
    double alpha_{1.0};     // Pheromone importance in selection probability (tau ^ alpha)
    double beta_{2.0};      // Heuristic importance in selection probability (eta ^ beta)
    double rho_{0.1};       // Pheromone evaporation rate for global update
    double tau_min_{0.01};  // Minimum pheromone trail limit
    double tau_max_{1.0};   // Maximum pheromone trail limit
    int max_retries_{100};  // Maximum retries allowed to generate a feasible solution
    // NOLINTEND(*-magic-numbers)
  };  // struct ACO

  /**
   * @brief Hyperparameters controlling the Local Search behavior.
   */
  struct LS {
    LS();

    [[nodiscard]] auto prob_1_move() const noexcept -> double { return prob_1_move_; }
    [[nodiscard]] auto max_improvements() const noexcept -> int { return max_improvements_; }
    [[nodiscard]] auto patience() const noexcept -> int { return patience_; }

    void set_prob_1_move(double prob_1_move) {
      if (!std::isfinite(prob_1_move) || prob_1_move < 0.0 || prob_1_move > 1.0) {
        throw std::invalid_argument(
            std::format("LS.prob_1_move must be finite and in [0, 1] (got: {})", prob_1_move));
      }
      prob_1_move_ = prob_1_move;
    }

    void set_max_improvements(int max_improvements) {
      if (max_improvements < 0) {
        throw std::invalid_argument(
            std::format("LS.max_improvements must be non-negative (got: {})", max_improvements));
      }
      max_improvements_ = max_improvements;
    }

    void set_patience(int patience) {
      if (patience < 0) {
        throw std::invalid_argument(
            std::format("LS.patience must be non-negative (got: {})", patience));
      }
      patience_ = patience;
    }

   private:
    // NOLINTBEGIN(*-magic-numbers)
    double prob_1_move_{0.5};   // Probability of applying 1-move operator instead of 2-swap
    int max_improvements_{10};  // Maximum number of actual successful moves (downhill steps)
    int patience_{100};         // Stop early after this many consecutive non-improving attempts
    // NOLINTEND(*-magic-numbers)
  };  // struct LS

  struct RoomAssignment {
    RoomAssignment();

    [[nodiscard]] auto max_time_seconds() const noexcept -> double { return max_time_seconds_; }
    [[nodiscard]] auto num_workers() const noexcept -> int { return num_workers_; }
    [[nodiscard]] auto random_seed() const noexcept -> int { return random_seed_; }

    void set_max_time_seconds(double seconds) {
      if (!std::isfinite(seconds) || seconds <= 0.0) {
        throw std::invalid_argument(std::format(
            "RoomAssignment.max_time_seconds must be finite and positive (got: {})", seconds));
      }
      max_time_seconds_ = seconds;
    }

    void set_num_workers(int workers) {
      if (workers <= 0) {
        throw std::invalid_argument(
            std::format("RoomAssignment.num_workers must be positive (got: {})", workers));
      }
      num_workers_ = workers;
    }

    void set_random_seed(int seed) {
      if (seed < 0) {
        throw std::invalid_argument(
            std::format("RoomAssignment.random_seed must be non-negative (got: {})", seed));
      }
      random_seed_ = seed;
    }

   private:
    // NOLINTBEGIN(*-magic-numbers)
    double max_time_seconds_{30.0};  // Shared budget for both lexicographic optimization phases.
    int num_workers_{1};
    int random_seed_{42};
    // NOLINTEND(*-magic-numbers)
  };  // struct RoomAssignment

  // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
  Evaluation eval;
  ACO aco;
  LS ls;
  RoomAssignment room_assignment;
  // NOLINTEND(misc-non-private-member-variables-in-classes)

  explicit Hyperparams(Evaluation _eval = Evaluation(), ACO _aco = ACO(), LS _ls = LS(),
                       RoomAssignment _room_assignment = RoomAssignment())
      : eval(_eval), aco(_aco), ls(_ls), room_assignment(_room_assignment) {}
};  // struct Hyperparams

inline Hyperparams::Evaluation::Evaluation() = default;
inline Hyperparams::ACO::ACO() = default;
inline Hyperparams::LS::LS() = default;
inline Hyperparams::RoomAssignment::RoomAssignment() = default;

}  // namespace common
