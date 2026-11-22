#pragma once

#include <cmath>
#include <format>
#include <stdexcept>

namespace cetp::slots {

struct EvalParams {
  [[nodiscard]] auto penalty_decay_base() const noexcept -> double { return penalty_decay_base_; }

  void set_penalty_decay_base(double penalty_decay_base) {
    if (!std::isfinite(penalty_decay_base) || penalty_decay_base <= 1.0) {
      throw std::invalid_argument(
          std::format(
              "Evaluation.penalty_decay_base must be finite and greater than 1.0 (got: {})",
              penalty_decay_base
          )
      );
    }
    penalty_decay_base_ = penalty_decay_base;
  }

private:
  // NOLINTBEGIN(*-magic-numbers)

  // Base for the exponential decay of time-gap penalties (base ^ -delta_days).
  // Controls how fast the penalty drops: 2.0 halves the penalty each day.
  double penalty_decay_base_ = 2.0;
  // NOLINTEND(*-magic-numbers)
};

}  // namespace cetp::slots
