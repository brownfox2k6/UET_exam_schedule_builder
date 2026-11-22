#pragma once

#include <cmath>
#include <format>
#include <stdexcept>

namespace cetp::rooms {

class Params {
public:
  [[nodiscard]] auto max_time_seconds() const noexcept -> double { return max_time_seconds_; }
  [[nodiscard]] auto num_workers() const noexcept -> int { return num_workers_; }
  [[nodiscard]] auto random_seed() const noexcept -> int { return random_seed_; }

  void set_max_time_seconds(double seconds) {
    if (!std::isfinite(seconds) || seconds <= 0.0) {
      throw std::invalid_argument(
          std::format(
              "RoomAssignment.max_time_seconds must be finite and positive (got: {})",
              seconds
          )
      );
    }
    max_time_seconds_ = seconds;
  }

  void set_num_workers(int workers) {
    if (workers <= 0) {
      throw std::invalid_argument(
          std::format("RoomAssignment.num_workers must be positive (got: {})", workers)
      );
    }
    num_workers_ = workers;
  }

  void set_random_seed(int seed) {
    if (seed < 0) {
      throw std::invalid_argument(
          std::format("RoomAssignment.random_seed must be non-negative (got: {})", seed)
      );
    }
    random_seed_ = seed;
  }

private:
  // NOLINTBEGIN(*-magic-numbers)
  double max_time_seconds_ = 30.0;  // Shared budget for both lexicographic optimization phases.
  int num_workers_ = 1;
  int random_seed_ = 42;
  // NOLINTEND(*-magic-numbers)
};

}  // namespace cetp::rooms
