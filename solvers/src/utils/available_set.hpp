#pragma once

#include <cassert>
#include <random>
#include <utility>

#include "utils/csr_matrix.hpp"
#include "utils/matrix.hpp"
#include "utils/span.hpp"
#include "utils/vector.hpp"

/**
 * @brief A data structure to manage a set of available options for multiple items.
 * * It provides O(1) random access and efficient addition/removal of options using the
 * "swap-to-back" technique. This is particularly useful for heuristics like Saturation Degree.
 */
struct AvailableSet {
private:
  // row = item, col = list of current available options
  CsrMatrix<int> options;

  // Track position of options: `pos(item, option) = index`
  Matrix<int> pos;

  // Current number of available options for item i: `active_ends[i+1] - options.offsets[i]`
  Vector<int> active_ends;

public:
  AvailableSet(const CsrMatrix<int>& available_original, int num_options_count)
      : options(available_original),
        pos(available_original.num_rows(), num_options_count) {
    reset();
  }

  /**
   * @brief Resets all items to their initial available sets from the CSR input.
   */
  void reset() {
    pos.fill(-1);
    active_ends = options.get_offsets();
    for (int item = 0; item < options.num_rows(); ++item) {
      int index = 0;
      for (int option : options[item].values) {
        pos[item, option] = index++;
      }
    }
  }

  /**
   * @brief Gets the number of currently available options for an item.
   */
  [[nodiscard]] auto get_available_count(int item) const -> int {
    assert(item >= 0 && item < options.num_rows());
    return active_ends[item + 1] - options.get_offsets()[item];
  }

  /**
   * @brief Gets the number of currently unavailable options for an item.
   */
  [[nodiscard]] auto get_unavailable_count(int item) const -> int {
    return options[item].values.size() - get_available_count(item);
  }

  /**
   * @brief Checks if `option` is in `item` 's initial available set
   */
  [[nodiscard]] auto has_option(int item, int option) const -> bool {
    return pos[item, option] != -1;
  }

  /**
   * @brief Checks if `option` of `item` is currently available
   */
  [[nodiscard]] auto is_available(int item, int option) const -> bool {
    const int index = pos[item, option];
    return index >= 0 && index < get_available_count(item);
  }

  /**
   * @brief Selects a random available option for an item in O(1).
   */
  [[nodiscard]] auto get_random(int item, std::mt19937& rng) const -> int {
    const int count = get_available_count(item);
    assert(count > 0);
    std::uniform_int_distribution<int> dist(0, count - 1);
    return options[item].values[dist(rng)];
  }

  /**
   * @brief Removes an option from the available set of an item in O(1).
   */
  void remove(int item, int option) {
    const int count = get_available_count(item);
    const int index = pos[item, option];
    assert(index >= 0 && index < count && "Option must be active before removal");
    auto row = options[item].values;
    std::swap(pos[item, option], pos[item, row[count - 1]]);
    std::swap(row[index], row[count - 1]);
    --active_ends[item + 1];
  }

  /**
   * @brief Restores an option to the available set of an item in O(1).
   */
  void restore(int item, int option) {
    const int count = get_available_count(item);
    const int index = pos[item, option];
    assert(index >= 0 && index >= count && "Option must exist and be inactive before restoration");
    auto row = options[item].values;
    std::swap(pos[item, option], pos[item, row[count]]);
    std::swap(row[index], row[count]);
    ++active_ends[item + 1];
  }

  /**
   * @brief Accesses the element at `[item, index]` by value (read-only).
   */
  auto operator[](int item, int index) const -> int {
    assert(index >= 0 && index < get_available_count(item));
    return options[item].values[index];
  }

  /**
   * @brief Accesses the available options of an item as a span (read-only).
   */
  auto operator[](int item) const -> Span<const int> {
    const auto row = options[item].values;
    const int count = get_available_count(item);
    return row.subspan(0, count);
  }
};
