#pragma once

#include <cstddef>
#include <random>
#include <utility>

#include "matrix.hpp"
#include "utils/assert.hpp"

namespace utils {

/**
 * @brief A data structure to manage a set of feasible options for multiple items.
 * * It provides O(1) random access and efficient addition/removal of options using the
 * "swap-to-back" technique. This is particularly useful for heuristics like Saturation Degree.
 */
struct FeasibleSet {
 private:
  // Tells how many different values in options
  int num_options;

  // row = item, col = list of current feasible options
  CsrMatrix<int> options;

  // Track position of options: `pos(item, option) = index`
  Matrix<int> pos;

  // Current number of feasible options for item i: `active_ends[i+1] - options.offsets[i]`
  std::vector<int> active_ends;

 public:
  FeasibleSet(const CsrMatrix<int>& feasible_original, int num_options)
      : num_options(num_options),
        options(feasible_original),
        pos(feasible_original.num_rows(), num_options) {
    reset();
  }

  /**
   * @brief Resets all items to their initial feasible sets from the CSR input.
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
   * @brief Gets the number of currently feasible options for an item.
   */
  template <typename T>
    requires std::is_integral_v<T>
  [[nodiscard]] auto get_feasible_count(T item) const -> int {
    PANIC_IF(std::cmp_less(item, 0) || std::cmp_greater_equal(item, options.num_rows()),
             "Item index {} out of bounds [0, {}]", item, options.num_rows() - 1);
    return active_ends[size_t(item + 1)] - options.get_offsets()[size_t(item)];
  }

  /**
   * @brief Gets the number of currently forbidden options for an item.
   */
  template <typename T>
    requires std::is_integral_v<T>
  [[nodiscard]] auto get_forbidden_count(T item) const -> int {
    PANIC_IF(std::cmp_less(item, 0) || std::cmp_greater_equal(item, options.num_rows()),
             "Item index {} out of bounds [0, {}]", item, options.num_rows() - 1);
    return int(options[item].values.size()) - get_feasible_count(item);
  }

  /**
   * @brief Checks if `option` is in `item` 's initial feasible set
   */
  template <typename T1, typename T2>
    requires std::is_integral_v<T1> && std::is_integral_v<T2>
  [[nodiscard]] auto has_option(T1 item, T2 option) const -> bool {
    PANIC_IF(std::cmp_less(item, 0) || std::cmp_greater_equal(item, options.num_rows()),
             "Item index {} out of bounds [0, {}]", item, options.num_rows() - 1);
    PANIC_IF(std::cmp_less(option, 0) || std::cmp_greater_equal(option, num_options),
             "Option {} out of bounds [0, {}]", option, num_options - 1);
    return pos[item, option] != -1;
  }

  /**
   * @brief Checks if `option` of `item` is currently active
   */
  template <typename T1, typename T2>
    requires std::is_integral_v<T1> && std::is_integral_v<T2>
  [[nodiscard]] auto is_active(T1 item, T2 option) const -> bool {
    PANIC_IF(std::cmp_less(item, 0) || std::cmp_greater_equal(item, options.num_rows()),
             "Item index {} out of bounds [0, {}]", item, options.num_rows() - 1);
    PANIC_IF(std::cmp_less(option, 0) || std::cmp_greater_equal(option, num_options),
             "Option {} out of bounds [0, {}]", option, num_options - 1);
    const int index = pos[item, option];
    return index >= 0 && index < get_feasible_count(item);
  }

  /**
   * @brief Selects a random feasible option for an item in O(1).
   */
  template <typename T>
    requires std::is_integral_v<T>
  [[nodiscard]] auto get_random(T item, std::mt19937& rng) const -> int {
    PANIC_IF(std::cmp_less(item, 0) || std::cmp_greater_equal(item, options.num_rows()),
             "Item index {} out of bounds [0, {}]", item, options.num_rows() - 1);
    const int count = get_feasible_count(item);
    PANIC_IF(count == 0, "Item {}: No feasible options left to pick from", item);
    std::uniform_int_distribution<int> dist(0, count - 1);
    return options[item].values[size_t(dist(rng))];
  }

  /**
   * @brief Removes an option from the feasible set of an item in O(1).
   */
  template <typename T1, typename T2>
    requires std::is_integral_v<T1> && std::is_integral_v<T2>
  void remove(T1 item, T2 option) {
    PANIC_IF(std::cmp_less(item, 0) || std::cmp_greater_equal(item, options.num_rows()),
             "Item index {} out of bounds [0, {}]", item, options.num_rows() - 1);
    PANIC_IF(std::cmp_less(option, 0) || std::cmp_greater_equal(option, num_options),
             "Option {} out of bounds [0, {}]", option, num_options - 1);
    const int count = get_feasible_count(item);
    const int index = pos[item, option];
    PANIC_IF(std::cmp_less(index, 0), "Item {} does not contain {} in its initial feasible set",
             item, option);
    PANIC_IF(std::cmp_greater_equal(index, count), "Item {}: Option {} is already inactive", item,
             option);
    auto row = options[item].values;
    std::swap(pos[item, option], pos[item, row[size_t(count - 1)]]);
    std::swap(row[size_t(index)], row[size_t(count - 1)]);
    --active_ends[size_t(item + 1)];
  }

  /**
   * @brief Restores an option to the feasible set of an item in O(1).
   */
  template <typename T1, typename T2>
    requires std::is_integral_v<T1> && std::is_integral_v<T2>
  void restore(T1 item, T2 option) {
    PANIC_IF(std::cmp_less(item, 0) || std::cmp_greater_equal(item, options.num_rows()),
             "Item index {} out of bounds [0, {}]", item, options.num_rows() - 1);
    PANIC_IF(std::cmp_less(option, 0) || std::cmp_greater_equal(option, num_options),
             "Option {} out of bounds [0, {}]", option, num_options - 1);
    const int count = get_feasible_count(item);
    const int index = pos[item, option];
    PANIC_IF(std::cmp_less(index, 0), "Item {} does not contain {} in its initial feasible set",
         item, option);
    PANIC_IF(index < count, "Item {}: Option {} is already active", item, option);
    auto row = options[item].values;
    std::swap(pos[item, option], pos[item, row[size_t(count)]]);
    std::swap(row[size_t(index)], row[size_t(count)]);
    ++active_ends[size_t(item + 1)];
  }

  /**
   * @brief Accesses the element at `[item, index]` by value (read-only).
   */
  template <typename T1, typename T2>
    requires std::is_integral_v<T1> && std::is_integral_v<T2>
  auto operator[](T1 item, T2 index) const -> int {
    PANIC_IF(std::cmp_less(item, 0) || std::cmp_greater_equal(item, options.num_rows()),
             "Item index {} out of bounds [0, {}]", item, options.num_rows() - 1);
    PANIC_IF(std::cmp_less(index, 0) || std::cmp_greater_equal(index, get_feasible_count(item)),
             "Item {}: Index {} out of feasible range", item, index);
    return options[item].values[size_t(index)];
  }

  template <typename T>
    requires std::is_integral_v<T>
  auto operator[](T item) const -> std::span<const int> {
    PANIC_IF(std::cmp_less(item, 0) || std::cmp_greater_equal(item, options.num_rows()),
             "Item index {} out of bounds [0, {}]", item, options.num_rows() - 1);
    const auto row = options[item].values;
    const int count = get_feasible_count(item);
    return row.subspan(0, count);
  }
};

}  // namespace utils