#pragma once

#include <cmath>
#include <format>
#include <stdexcept>

namespace cetp::slots::aco {

struct Params {
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
          std::format("ACO.num_iters must be positive (got: {})", num_iters)
      );
    }
    num_iters_ = num_iters;
  }

  void set_num_ants(int num_ants) {
    if (num_ants <= 0) {
      throw std::invalid_argument(std::format("ACO.num_ants must be positive (got: {})", num_ants));
    }
    num_ants_ = num_ants;
  }

  void set_alpha(double alpha) {
    if (!std::isfinite(alpha) || alpha < 0.0) {
      throw std::invalid_argument(
          std::format("ACO.alpha must be finite and non-negative (got: {})", alpha)
      );
    }
    alpha_ = alpha;
  }

  void set_beta(double beta) {
    if (!std::isfinite(beta) || beta < 0.0) {
      throw std::invalid_argument(
          std::format("ACO.beta must be finite and non-negative (got: {})", beta)
      );
    }
    beta_ = beta;
  }

  void set_rho(double rho) {
    if (!std::isfinite(rho) || rho < 0.0 || rho > 1.0) {
      throw std::invalid_argument(
          std::format("ACO.rho must be finite and in [0, 1] (got: {})", rho)
      );
    }
    rho_ = rho;
  }

  void set_tau_min(double tau_min) {
    if (!std::isfinite(tau_min) || tau_min <= 0.0 || tau_min >= tau_max_) {
      throw std::invalid_argument(
          std::format(
              "ACO.tau_min must be finite, positive, and below tau_max (got: {}, tau_max: {})",
              tau_min,
              tau_max_
          )
      );
    }
    tau_min_ = tau_min;
  }

  void set_tau_max(double tau_max) {
    if (!std::isfinite(tau_max) || tau_max <= tau_min_) {
      throw std::invalid_argument(
          std::format(
              "ACO.tau_max must be finite, positive, and above tau_min (got: {}, tau_min: {})",
              tau_max,
              tau_min_
          )
      );
    }
    tau_max_ = tau_max;
  }

  void set_tau_bounds(double tau_min, double tau_max) {
    if (!std::isfinite(tau_max) || !std::isfinite(tau_min) || tau_min <= 0.0
        || tau_min >= tau_max) {
      throw std::invalid_argument(
          std::format(
              "ACO bounds satisfy 0 < tau_min < tau_max < INF (got: {}, {})",
              tau_min,
              tau_max
          )
      );
    }
    tau_min_ = tau_min;
    tau_max_ = tau_max;
  }

  void set_max_retries(int max_retries) {
    if (max_retries <= 0) {
      throw std::invalid_argument(
          std::format("ACO.max_retries must be positive (got: {})", max_retries)
      );
    }
    max_retries_ = max_retries;
  }

private:
  // NOLINTBEGIN(*-magic-numbers)
  int num_iters_ = 100;    // Number of iterations
  int num_ants_ = 10;      // Number of ants used in each iteration
  double alpha_ = 1.0;     // Pheromone importance in selection probability (tau ^ alpha)
  double beta_ = 2.0;      // Heuristic importance in selection probability (eta ^ beta)
  double rho_ = 0.1;       // Pheromone evaporation rate for global update
  double tau_min_ = 0.01;  // Minimum pheromone trail limit
  double tau_max_ = 1.0;   // Maximum pheromone trail limit
  int max_retries_ = 100;  // Maximum retries allowed to generate a feasible solution
  // NOLINTEND(*-magic-numbers)
};

}  // namespace cetp::slots::aco
