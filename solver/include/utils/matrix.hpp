#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include "utils/assert.hpp"

namespace utils {

/**
 * @brief A generic 2D matrix structure.
 * Implements a flat 1D vector back-end to ensure memory continuity.
 */
template <typename T>
struct Matrix {
 private:
  std::vector<T> data_;
  int num_rows_;
  int num_cols_;

 public:
  /**
   * @brief Initializes a matrix of size r x c with a default value.
   */
  template <typename R, typename C>
    requires std::is_integral_v<R> && std::is_integral_v<C>
  Matrix(R rows, C cols, T value = T()) {
    PANIC_IF(std::cmp_less_equal(rows, 0), "Matrix rows must be positive (got: {})", rows);
    PANIC_IF(std::cmp_less_equal(cols, 0), "Matrix columns must be positive (got: {})", cols);
    num_rows_ = int(rows);
    num_cols_ = int(cols);
    data_.assign(size_t(rows) * size_t(cols), value);
  }

  /**
   * @brief Constructs a matrix from a 2D std::vector (list of lists).
   */
  Matrix(const std::vector<std::vector<T>>& list) {
    PANIC_IF(list.empty(), "Matrix list must not be empty");
    PANIC_IF(list[0].empty(), "Matrix list rows must not be empty");
    num_rows_ = int(list.size());
    num_cols_ = int(list[0].size());
    data_.assign(size_t(num_rows_) * size_t(num_cols_), T());
    int element_index = 0;
    for (int row_index = 0; row_index < num_rows_; ++row_index) {
      const std::vector<T>& row = list[row_index];
      PANIC_IF(row.size() != num_cols_, "Matrix row {} has size {}, expected {}", row_index,
               row.size(), num_cols_);
      for (const T& element : row) {
        data_[element_index++] = element;
      }
    }
  }

  [[nodiscard]] auto data() const -> const std::vector<T>& { return data_; }
  [[nodiscard]] auto num_rows() const -> int { return num_rows_; }
  [[nodiscard]] auto num_cols() const -> int { return num_cols_; }
  [[nodiscard]] auto num_elements() const -> int { return int(data_.size()); }

  /**
   * @brief Accesses the element at `[row, col]`.
   */
  template <typename R, typename C>
    requires std::is_integral_v<R> && std::is_integral_v<C>
  auto operator[](R row, C col) -> T& {
    PANIC_IF(std::cmp_less(row, 0) || std::cmp_greater_equal(row, num_rows_),
             "Row index {} out of bounds [0, {}]", row, num_rows_);
    PANIC_IF(std::cmp_less(col, 0) || std::cmp_greater_equal(col, num_cols_),
             "Column index {} out of bounds [0, {}]", col, num_cols_);
    return data_[(size_t(row) * size_t(num_cols_)) + size_t(col)];
  }

  template <typename R, typename C>
    requires std::is_integral_v<R> && std::is_integral_v<C>
  auto operator[](R row, C col) const -> const T& {
    PANIC_IF(std::cmp_less(row, 0) || std::cmp_greater_equal(row, num_rows_),
             "Row index {} out of bounds [0, {}]", row, num_rows_);
    PANIC_IF(std::cmp_less(col, 0) || std::cmp_greater_equal(col, num_cols_),
             "Column index {} out of bounds [0, {}]", col, num_cols_);
    return data_[(size_t(row) * size_t(num_cols_)) + size_t(col)];
  }

  /**
   * @brief Fills the entire matrix with the specified value.
   */
  void fill(T value) { std::ranges::fill(data_, value); }
};

template <typename T>
struct CsrRowView {
  std::span<const int> indices;  // NOLINT(misc-non-private-member-variables-in-classes)
  std::span<T> values;           // NOLINT(misc-non-private-member-variables-in-classes)

  [[nodiscard]] auto size() const -> int { return int(values.size()); }
};

/**
 * @brief A generic Compressed Sparse Row matrix structure.
 * Implements flat 1D vectors for data and offsets to ensure memory continuity and fast,
 * cache-friendly row iteration.
 */
template <typename T>
struct CsrMatrix {
 private:
  std::vector<int> indices_;
  std::vector<T> values_;
  std::vector<int> offsets_;
  bool has_indices_;

 public:
  /**
   * @brief Constructs a CSR matrix from a dense matrix.
   */
  template <typename S>
    requires std::is_integral_v<S>
  CsrMatrix(const Matrix<T>& matrix, S expected_size = 0, bool track_indices = true,
            T trash_value = T())
      : has_indices_(track_indices) {
    if (has_indices_) {
      indices_.reserve(size_t(expected_size));
    }
    values_.reserve(size_t(expected_size));
    offsets_.assign(size_t(matrix.num_rows()) + 1, 0);
    size_t index = 0;
    const std::vector<T>& matrix_data = matrix.data();
    for (size_t i = 0; std::cmp_less(i, matrix.num_rows()); ++i) {
      for (int j = 0; j < matrix.num_cols(); ++j) {
        T value = matrix_data[index++];
        if (value == trash_value) {
          continue;
        }
        if (has_indices_) {
          indices_.emplace_back(j);
        }
        values_.emplace_back(value);
      }
      offsets_[i + 1] = int(values_.size());
    }
    if (has_indices_) {
      indices_.shrink_to_fit();
    }
    values_.shrink_to_fit();
  }

  CsrMatrix(const Matrix<T>& matrix) : CsrMatrix<T>(matrix, 0, true, T()) {}

  /**
   * @brief Constructs a CSR matrix from a 2D std::vector (list of lists).
   */
  template <typename S>
    requires std::is_integral_v<S>
  CsrMatrix(const std::vector<std::vector<T>>& list, S expected_size = 0, bool track_indices = true,
            T trash_value = T())
      : has_indices_(track_indices) {
    if (has_indices_) {
      indices_.reserve(size_t(expected_size));
    }
    values_.reserve(size_t(expected_size));
    offsets_.assign(list.size() + 1, 0);
    for (size_t i = 0; i < list.size(); ++i) {
      const std::vector<T>& row = list[i];
      for (size_t j = 0; j < row.size(); ++j) {
        if (row[j] == trash_value) {
          continue;
        }
        if (has_indices_) {
          indices_.emplace_back(int(j));
        }
        values_.emplace_back(row[j]);
      }
      offsets_[i + 1] = int(values_.size());
    }
    if (has_indices_) {
      indices_.shrink_to_fit();
    }
    values_.shrink_to_fit();
  }

  [[nodiscard]] auto num_rows() const -> int { return int(offsets_.size()) - 1; }
  [[nodiscard]] auto num_elements() const -> int { return int(values_.size()); }
  [[nodiscard]] auto get_offsets() const -> const std::vector<int>& { return offsets_; }

  /**
   * @brief Access a specific row of the matrix.
   */
  template <typename R>
    requires std::is_integral_v<R>
  auto operator[](R row_index) -> CsrRowView<T> {
    PANIC_IF(std::cmp_less(row_index, 0) || std::cmp_greater_equal(row_index, num_rows()),
             "Row index {} out of bounds [0, {}]", row_index, num_rows() - 1);
    const auto row = size_t(row_index);
    const int start = offsets_[row];
    const int end = offsets_[row + 1];
    return CsrRowView<T>{
        .indices = has_indices_
                       ? std::span<const int>{indices_}.subspan(size_t(start), size_t(end - start))
                       : std::span<const int>{},
        .values = std::span<T>{values_}.subspan(size_t(start), size_t(end - start))};
  }

  template <typename R>
    requires std::is_integral_v<R>
  auto operator[](R row_index) const -> CsrRowView<const T> {
    PANIC_IF(std::cmp_less(row_index, 0) || std::cmp_greater_equal(row_index, num_rows()),
             "Row index {} out of bounds [0, {}]", row_index, num_rows() - 1);
    const auto row = size_t(row_index);
    const int start = offsets_[row];
    const int end = offsets_[row + 1];
    return CsrRowView<const T>{
        .indices = has_indices_
                       ? std::span<const int>{indices_}.subspan(size_t(start), size_t(end - start))
                       : std::span<const int>{},
        .values = std::span<const T>{values_}.subspan(size_t(start), size_t(end - start))};
  }
};

}  // namespace utils