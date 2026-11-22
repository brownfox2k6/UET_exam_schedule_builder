#pragma once

#include <cmath>
#include <format>
#include <stdexcept>

namespace cetp::slots::local_search {

struct Params {
  [[nodiscard]] auto prob_1_move() const noexcept -> double { return prob_1_move_; }
  [[nodiscard]] auto max_improvements() const noexcept -> int { return max_improvements_; }
  [[nodiscard]] auto patience() const noexcept -> int { return patience_; }

  void set_prob_1_move(double prob_1_move) {
    if (!std::isfinite(prob_1_move) || prob_1_move < 0.0 || prob_1_move > 1.0) {
      throw std::invalid_argument(
          std::format("LS.prob_1_move must be finite and in [0, 1] (got: {})", prob_1_move)
      );
    }
    prob_1_move_ = prob_1_move;
  }

  void set_max_improvements(int max_improvements) {
    if (max_improvements < 0) {
      throw std::invalid_argument(
          std::format("LS.max_improvements must be non-negative (got: {})", max_improvements)
      );
    }
    max_improvements_ = max_improvements;
  }

  void set_patience(int patience) {
    if (patience < 0) {
      throw std::invalid_argument(
          std::format("LS.patience must be non-negative (got: {})", patience)
      );
    }
    patience_ = patience;
  }

private:
  // NOLINTBEGIN(*-magic-numbers)
  double prob_1_move_ = 0.5;   // Probability of applying 1-move operator instead of 2-swap
  int max_improvements_ = 10;  // Maximum number of actual successful moves (downhill steps)
  int patience_ = 100;         // Stop early after this many consecutive non-improving attempts
  // NOLINTEND(*-magic-numbers)
};

}  // namespace cetp::slots::local_search
