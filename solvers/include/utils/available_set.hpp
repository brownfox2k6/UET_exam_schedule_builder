#pragma once

#include <cstddef>
#include <random>
#include <utility>

#include "utils/assert.hpp"
#include "utils/matrix.hpp"

/**
 * @brief A data structure to manage a set of available options for multiple items.
 * * It provides O(1) random access and efficient addition/removal of options using the
 * "swap-to-back" technique. This is particularly useful for heuristics like Saturation Degree.
 */
struct AvailableSet {
 private:
  // Tells how many different values in options
  [[maybe_unused]] int num_options;

  // row = item, col = list of current available options
  CsrMatrix<int> options;

  // Track position of options: `pos(item, option) = index`
  Matrix<int> pos;

  // Current number of available options for item i: `active_ends[i+1] - options.offsets[i]`
  Vector<int> active_ends;

 public:
  AvailableSet(const CsrMatrix<int>& available_original, int num_options_count)
      : num_options(num_options_count),
        options(available_original),
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
        PANIC_IF(option < 0 || option >= num_options, "Option value {} out of bounds [0, {}]",
                 option, num_options - 1);
        pos[item, option] = index++;
      }
    }
  }

  /**
   * @brief Gets the number of currently available options for an item.
   */
  [[nodiscard]] auto get_available_count(int item) const -> int {
    PANIC_IF(item < 0 || item >= options.num_rows(), "Item index {} out of bounds [0, {}]", item,
             options.num_rows() - 1);
    return active_ends[item + 1] - options.get_offsets()[item];
  }

  /**
   * @brief Gets the number of currently unavailable options for an item.
   */
  [[nodiscard]] auto get_unavailable_count(int item) const -> int {
    PANIC_IF(item < 0 || item >= options.num_rows(), "Item index {} out of bounds [0, {}]", item,
             options.num_rows() - 1);
    return int(options[item].values.size()) - get_available_count(item);
  }

  /**
   * @brief Checks if `option` is in `item` 's initial available set
   */
  [[nodiscard]] auto has_option(int item, int option) const -> bool {
    PANIC_IF(item < 0 || item >= options.num_rows(), "Item index {} out of bounds [0, {}]", item,
             options.num_rows() - 1);
    PANIC_IF(option < 0 || option >= num_options, "Option {} out of bounds [0, {}]", option,
             num_options - 1);
    return pos[item, option] != -1;
  }

  /**
   * @brief Checks if `option` of `item` is currently available
   */
  [[nodiscard]] auto is_available(int item, int option) const -> bool {
    PANIC_IF(item < 0 || item >= options.num_rows(), "Item index {} out of bounds [0, {}]", item,
             options.num_rows() - 1);
    PANIC_IF(option < 0 || option >= num_options, "Option {} out of bounds [0, {}]", option,
             num_options - 1);
    const int index = pos[item, option];
    return index >= 0 && index < get_available_count(item);
  }

  /**
   * @brief Selects a random available option for an item in O(1).
   */
  [[nodiscard]] auto get_random(int item, std::mt19937& rng) const -> int {
    PANIC_IF(item < 0 || item >= options.num_rows(), "Item index {} out of bounds [0, {}]", item,
             options.num_rows() - 1);
    const int count = get_available_count(item);
    PANIC_IF(count == 0, "Item {}: No available options left to pick from", item);
    std::uniform_int_distribution<int> dist(0, count - 1);
    return options[item].values[size_t(dist(rng))];
  }

  /**
   * @brief Removes an option from the available set of an item in O(1).
   */
  void remove(int item, int option) {
    PANIC_IF(item < 0 || item >= options.num_rows(), "Item index {} out of bounds [0, {}]", item,
             options.num_rows() - 1);
    PANIC_IF(option < 0 || option >= num_options, "Option {} out of bounds [0, {}]", option,
             num_options - 1);
    const int count = get_available_count(item);
    const int index = pos[item, option];
    PANIC_IF(index < 0, "Item {} does not contain {} in its initial available set", item, option);
    PANIC_IF(index >= count, "Item {}: Option {} is already inactive", item, option);
    auto row = options[item].values;
    std::swap(pos[item, option], pos[item, row[size_t(count - 1)]]);
    std::swap(row[size_t(index)], row[size_t(count - 1)]);
    --active_ends[item + 1];
  }

  /**
   * @brief Restores an option to the available set of an item in O(1).
   */
  void restore(int item, int option) {
    PANIC_IF(item < 0 || item >= options.num_rows(), "Item index {} out of bounds [0, {}]", item,
             options.num_rows() - 1);
    PANIC_IF(option < 0 || option >= num_options, "Option {} out of bounds [0, {}]", option,
             num_options - 1);
    const int count = get_available_count(item);
    const int index = pos[item, option];
    PANIC_IF(index < 0, "Item {} does not contain {} in its initial available set", item, option);
    PANIC_IF(index < count, "Item {}: Option {} is already active", item, option);
    auto row = options[item].values;
    std::swap(pos[item, option], pos[item, row[size_t(count)]]);
    std::swap(row[size_t(index)], row[size_t(count)]);
    ++active_ends[item + 1];
  }

  /**
   * @brief Accesses the element at `[item, index]` by value (read-only).
   */
  auto operator[](int item, int index) const -> int {
    PANIC_IF(item < 0 || item >= options.num_rows(), "Item index {} out of bounds [0, {}]", item,
             options.num_rows() - 1);
    PANIC_IF(index < 0 || index >= get_available_count(item),
             "Item {}: Index {} out of available range", item, index);
    return options[item].values[size_t(index)];
  }

  /**
   * @brief Accesses the available options of an item as a span (read-only).
   */
  auto operator[](int item) const -> std::span<const int> {
    PANIC_IF(item < 0 || item >= options.num_rows(), "Item index {} out of bounds [0, {}]", item,
             options.num_rows() - 1);
    const auto row = options[item].values;
    const int count = get_available_count(item);
    return row.subspan(0, size_t(count));
  }
};
