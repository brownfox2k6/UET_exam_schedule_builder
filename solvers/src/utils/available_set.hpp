#pragma once

#include <cassert>
#include <random>
#include <utility>

#include "utils/jagged_array.hpp"
#include "utils/matrix.hpp"
#include "utils/random.hpp"
#include "utils/span.hpp"
#include "utils/vector.hpp"

/**
 * Manages a dynamic set of available options for multiple items using the "swap-to-back" technique.
 * Provides O(1) random sampling, membership checks, removal, and restoration.
 */
class AvailableSet {
public:
  AvailableSet(const JaggedArray<int>& available_original, int num_options)
      : options_(available_original),
        positions_(available_original.num_rows(), num_options) {
    reset();
  }

  /** Resets all items to their initial available option sets. */
  void reset() {
    positions_.fill(-1);
    active_ends_ = options_.offsets();
    for (int item = 0; item < options_.num_rows(); ++item) {
      int index = 0;
      for (int option : options_[item]) {
        positions_[item, option] = index++;
      }
    }
  }

  /** Returns the number of currently available options for `item`. */
  [[nodiscard]] auto get_available_count(int item) const -> int {
    assert(item >= 0 && item < options_.num_rows());
    return active_ends_[item + 1] - options_.offsets()[item];
  }

  /** Returns the number of currently unavailable options for `item`. */
  [[nodiscard]] auto get_unavailable_count(int item) const -> int {
    return options_[item].size() - get_available_count(item);
  }

  /** Returns true if `option` belongs to `item`'s initial option set. */
  [[nodiscard]] auto has_option(int item, int option) const -> bool {
    return positions_[item, option] != -1;
  }

  /** Returns true if `option` is currently available for `item`. */
  [[nodiscard]] auto is_available(int item, int option) const -> bool {
    const int index = positions_[item, option];
    return index >= 0 && index < get_available_count(item);
  }

  /** Selects a uniformly random available option for `item` in O(1). */
  [[nodiscard]] auto get_random(int item, std::mt19937& random_engine) const -> int {
    const int count = get_available_count(item);
    assert(count > 0);
    const int index = random_int(random_engine, 0, count);
    return options_[item][index];
  }

  /** Removes an active `option` from `item`'s available set in O(1). */
  void remove(int item, int option) {
    const int count = get_available_count(item);
    const int index = positions_[item, option];
    assert(index >= 0 && index < count && "Option must be active before removal");
    auto row = options_[item];
    std::swap(positions_[item, option], positions_[item, row[count - 1]]);
    std::swap(row[index], row[count - 1]);
    --active_ends_[item + 1];
  }

  /** Restores an inactive `option` to `item`'s available set in O(1). */
  void restore(int item, int option) {
    const int count = get_available_count(item);
    const int index = positions_[item, option];
    assert(index >= 0 && index >= count && "Option must exist and be inactive before restoration");
    auto row = options_[item];
    std::swap(positions_[item, option], positions_[item, row[count]]);
    std::swap(row[index], row[count]);
    ++active_ends_[item + 1];
  }

  /** Returns the `index`-th currently available option for `item`. */
  [[nodiscard]] auto operator[](int item, int index) const -> int {
    assert(index >= 0 && index < get_available_count(item));
    return options_[item][index];
  }

  /** Returns a read-only span of currently available options for `item`. */
  [[nodiscard]] auto operator[](int item) const -> Span<const int> {
    const auto row = options_[item];
    const int count = get_available_count(item);
    return row.subspan(0, count);
  }

private:
  // Row `item` holds its options, with active options packed in `[0, available_count)`.
  JaggedArray<int> options_;

  // Tracks each option's current index inside its row: `positions_[item, option] = index`.
  Matrix<int> positions_;

  // Active prefix end inside `options_`:
  // `available_count(item) = active_ends_[item + 1] - options_.offsets()[item]`.
  Vector<int> active_ends_;
};
