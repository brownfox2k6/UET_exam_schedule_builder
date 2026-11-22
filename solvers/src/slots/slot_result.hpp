#pragma once

#include <cmath>
#include <functional>
#include <utility>

#include "utils/vector.hpp"

namespace cetp::slots {

/** Reports an iteration and the best penalty so far, initially positive infinity. */
using SlotProgressCallback = std::function<void(int, double)>;

class SlotResult {
public:
  SlotResult(Vector<int> schedule, double penalty)
      : schedule_(std::move(schedule)),
        penalty_(penalty) {}

  /** Returns true if a valid schedule was found. */
  [[nodiscard]] auto found() const noexcept -> bool { return !std::isinf(penalty_); }

  /** Access the schedule when found() is true. */
  [[nodiscard]] auto schedule() const noexcept -> const Vector<int>& { return schedule_; }

  [[nodiscard]] auto penalty() const noexcept -> double { return penalty_; }

private:
  Vector<int> schedule_;
  double penalty_;
};

}  // namespace cetp::slots
