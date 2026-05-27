#pragma once

#include <random>

#include "matrix.hpp"
#include "utils/assert.hpp"

namespace utils {

/**
 * @brief A data structure to manage a set of feasible options for multiple items.
 * * It provides O(1) random access and efficient addition/removal of options 
 * using the "swap-to-back" technique. This is particularly useful for 
 * heuristics like Saturation Degree.
 * * @tparam T The type of the option value (e.g., int for slot IDs).
 */
template<typename T>
struct FeasibleSet {
private:
  const int num_items;      // Number of items
  const int num_options;    // Maximum number of possible options
  Matrix<T> options;        // 2D storage: row = item, col = list of current feasible options
  Matrix<int> pos;          // Track position of values: `pos(item, value) = index`
  std::vector<int> counts;  // Current number of feasible options for each item

public:
  FeasibleSet(int n_items, int n_options)
    : num_items(n_items),
      num_options(n_options),
      options(num_items, num_options),
      pos(num_items, num_options),
      counts(num_items)
  {
    reset();
  }

  /**
   * @brief Resets all items to their initial state where all options `[0, num_options)` are feasible.
   */
  void reset() {
    std::fill(counts.begin(), counts.end(), num_options);
    for (int i = 0; i < num_items; ++i) {
      for (int j = 0; j < num_options; ++j) {
        options(i, j) = static_cast<T>(j);
        pos(i, j) = j;
      }
    }
  }

  /**
   * @brief Gets the number of currently feasible options for an item.
   */
  int get_feasible_count(int item) const {
    return counts[item];
  }

  /**
   * @brief Gets the number of currently forbidden options for an item.
   */
  int get_forbidden_count(int item) const {
    return num_options - counts[item];
  }

  /**
   * @brief Selects a random feasible option for an item in O(1).
   */
  int get_random(int item, std::mt19937& rng) const {
    int count = counts[item];
    utils::panic_if(count <= 0, "Item {}: No feasible options left to pick from", item);
    std::uniform_int_distribution<int> dist(0, count - 1);
    return options(item, dist(rng));
  }

  /**
   * @brief Removes an option from the feasible set of an item in O(1).
   */
  void remove_option(int item, T value) {
    int &count = counts[item];
    int index = pos(item, value);
    utils::panic_if(index >= count, "Item {}: Value {} is already inactive or invalid", item, value);
    int last_val = options(item, count - 1);
    options(item, index) = last_val;
    options(item, count - 1) = value;
    pos(item, last_val) = index;
    pos(item, value) = count - 1;
    --count;
  }

  /**
   * @brief Restores an option to the feasible set of an item in O(1).
   */
  void add_option(int item, T value) {
    int &count = counts[item];
    int index = pos(item, value);
    utils::panic_if(index < count, "FeasibleSet: Item {}: Value {} is already active", item, value);
    int first_inactive_val = options(item, count);
    options(item, index) = first_inactive_val;
    options(item, count) = value;
    pos(item, first_inactive_val) = index;
    pos(item, value) = count;
    ++count;
  }

  /**
   * @brief Accesses the element at `(item, index)` by value (read-only). 
   */
  T& operator()(int item, int index) const {
    utils::panic_if(
      item < 0 || item >= num_items,
      "FeasibleSet: Item index out of range (got: {}, allowed: [0, {}])", item, num_items - 1
    );
    utils::panic_if(
      index < 0 || index >= counts[item],
      "FeasibleSet: Item {}: Index {} out of feasible range", item, index
    );
    return options(item, index);
  }

  std::span<const T> operator[](int item) const {
    utils::panic_if(
      item < 0 || item >= num_items,
      "FeasibleSet: Item index out of range (got: {}, allowed: [0, {}])", item, num_items - 1
    );
    return std::span<const T>(&options(item, 0), counts[item]);
  }
};

} // namespace utils