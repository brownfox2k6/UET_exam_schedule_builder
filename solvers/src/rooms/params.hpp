#pragma once

#include <cmath>
#include <format>
#include <stdexcept>

namespace cetp::rooms {

class Params {
public:
  [[nodiscard]] auto slot_time_limit() const noexcept -> double { return slot_time_limit_; }
  [[nodiscard]] auto num_workers() const noexcept -> int { return num_workers_; }

  void set_slot_time_limit(double seconds) {
    if (!std::isfinite(seconds) || seconds <= 0.0) {
      throw std::invalid_argument(
          std::format(
              "RoomAssignment.slot_time_limit must be finite and positive (got: {})",
              seconds
          )
      );
    }
    slot_time_limit_ = seconds;
  }

  void set_num_workers(int workers) {
    if (workers <= 0) {
      throw std::invalid_argument(
          std::format("RoomAssignment.num_workers must be positive (got: {})", workers)
      );
    }
    num_workers_ = workers;
  }

private:
  // NOLINTBEGIN(*-magic-numbers)

  // Time limit (in seconds) for solving each independent time slot.
  double slot_time_limit_ = 5.0;
  int num_workers_ = 1;

  // NOLINTEND(*-magic-numbers)
};

}  // namespace cetp::rooms
