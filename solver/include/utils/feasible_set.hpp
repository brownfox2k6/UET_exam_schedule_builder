#pragma once

#include "matrix.hpp"
#include "utils/assert.hpp"

#include <random>
#include <utility>

namespace utils {

/**
 * @brief A data structure to manage a set of feasible options for multiple items.
 * * It provides O(1) random access and efficient addition/removal of options 
 * using the "swap-to-back" technique. This is particularly useful for 
 * heuristics like Saturation Degree.
 */
struct FeasibleSet {
private:
  const int num_options;        // Tells how many different values in options
  CsrMatrix<int> options;       // row = item, col = list of current feasible options
  Matrix<int> pos;              // Track position of options: `pos(item, option) = index`
  std::vector<int> active_ends; // Current number of feasible options for item i: `active_ends[i+1] - options.offsets[i]`

  void check_bounds(std::string_view where, int item, int option = 0) const {
    PANIC_IF(
      item < 0 || item >= options.num_rows(),
      "{}: Item index {} out of bounds [0, {}]", where, item, options.num_rows() - 1
    );
    PANIC_IF(
      option < 0 || option >= num_options,
      "{}: Option {} out of bounds [0, {}]", where, option, num_options - 1
    );
  }

public:
  FeasibleSet(const CsrMatrix<int>& feasible_original, int num_options):
    num_options(num_options),
    options(feasible_original),
    pos(feasible_original.num_rows(), num_options)
  {
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
        PANIC_IF(
          option < 0 || option >= num_options,
          "FeasibleSet::reset: Option value {} out of bounds [0, {}]", option, num_options - 1
        );
        pos(item, option) = index++;
      }
    }
  }

  /**
   * @brief Gets the number of currently feasible options for an item.
   */
  int get_feasible_count(int item) const {
    check_bounds("FeasibleSet::get_feasible_count", item);
    return active_ends[item + 1] - options.get_offsets()[item];
  }

  /**
   * @brief Gets the number of currently forbidden options for an item.
   */
  int get_forbidden_count(int item) const {
    check_bounds("FeasibleSet::get_forbidden_count", item);
    return options[item].values.size() - get_feasible_count(item);
  }

  /**
   * @brief Checks if `option` is in `item` 's initial feasible set
   */
  bool has_option(int item, int option) const {
    check_bounds("FeasibleSet::has_option", item, option);
    return pos(item, option) != -1;
  }

  /**
   * @brief Checks if `option` of `item` is currently active
   */
  bool is_active(int item, int option) const {
    check_bounds("FeasibleSet::is_active", item, option);
    const int index = pos(item, option);
    if (index < 0) {
      return false;
    }
    return index < get_feasible_count(item);
  }

  /**
   * @brief Selects a random feasible option for an item in O(1).
   */
  int get_random(int item, std::mt19937& rng) const {
    check_bounds("FeasibleSet::get_random", item);
    int count = get_feasible_count(item);
    PANIC_IF(count == 0, "Item {}: No feasible options left to pick from", item);
    std::uniform_int_distribution<int> dist(0, count - 1);
    return options[item].values[dist(rng)];
  }

  /**
   * @brief Removes an option from the feasible set of an item in O(1).
   */
  void remove(int item, int option) {
    check_bounds("FeasibleSet::remove", item, option);
    const int count = get_feasible_count(item);
    const int index = pos(item, option);
    PANIC_IF(
      index < 0,
      "FeasibleSet::remove: Item {} does not contain {} in its initial feasible set", item, option
    );
    PANIC_IF(
      index >= count,
      "FeasibleSet::remove: Item {}: Option {} is already inactive", item, option
    );
    auto row = options[item].values;
    std::swap(pos(item, option), pos(item, row[count - 1]));
    std::swap(row[index], row[count - 1]);
    --active_ends[item + 1];
  }

  /**
   * @brief Restores an option to the feasible set of an item in O(1).
   */
  void restore(int item, int option) {
    check_bounds("FeasibleSet::restore", item, option);
    const int count = get_feasible_count(item);
    const int index = pos(item, option);
    PANIC_IF(
      index < 0,
      "FeasibleSet::restore: Item {} does not contain {} in its initial feasible set", item, option
    );
    PANIC_IF(
      index < count,
      "FeasibleSet::restore: Item {}: Option {} is already active", item, option
    );
    auto row = options[item].values;
    std::swap(pos(item, option), pos(item, row[count]));
    std::swap(row[index], row[count]);
    ++active_ends[item + 1];
  }

  /**
   * @brief Accesses the element at `(item, index)` by value (read-only). 
   */
  int operator()(int item, int index) const {
    check_bounds("FeasibleSet::operator()", item, 0);
    PANIC_IF(
      index < 0 || index >= get_feasible_count(item),
      "FeasibleSet::operator(): Item {}: Index {} out of feasible range", item, index
    );
    return options[item].values[index];
  }

  std::span<const int> operator[](int item) const {
    check_bounds("FeasibleSet::operator[]", item, 0);
    return {options[item].values.begin(), static_cast<size_t>(get_feasible_count(item))};
  }
};

} // namespace utils