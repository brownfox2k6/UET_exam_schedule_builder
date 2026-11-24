#pragma once

#include <cmath>
#include <functional>

#include "utils/vector.hpp"

namespace cetp::slots {

/** Reports a one-based iteration index and the best penalty so far (initially +INFINITY). */
using SlotProgressCallback = std::function<void(int, double)>;

/** Output of a slot-scheduling search or local-search improvement run. */
struct SlotResult {
  // NOLINTBEGIN(*-non-private-*)
  Vector<int> schedule;
  double penalty = INFINITY;
  // NOLINTEND(*-non-private-*)

  /** Returns true if a valid schedule was found. */
  [[nodiscard]] auto found() const noexcept -> bool { return !std::isinf(penalty); }
};  // struct SlotResult

}  // namespace cetp::slots
