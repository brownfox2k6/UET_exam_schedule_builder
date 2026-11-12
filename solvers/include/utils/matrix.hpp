#pragma once

#include <algorithm>
#include <utility>

#include "utils/assert.hpp"
#include "utils/span.hpp"
#include "utils/vector.hpp"

/**
 * @brief A generic 2D matrix structure.
 * Implements a flat 1D vector back-end to ensure memory continuity.
 */
template <typename T>
struct Matrix {
  /**
   * @brief Initializes a matrix of size r x c with a default value.
   */
  Matrix(int rows, int cols, T value = T()) {
    PANIC_IF(rows <= 0, "Matrix rows must be positive (got: {})", rows);
    PANIC_IF(cols <= 0, "Matrix columns must be positive (got: {})", cols);
    num_rows_ = rows;
    num_cols_ = cols;
    data_.assign(num_rows_ * num_cols_, value);
  }

  /**
   * @brief Constructs a matrix from a 2D Vector (list of lists).
   */
  Matrix(const Vector<Vector<T>>& list) {
    PANIC_IF(list.empty(), "Matrix list must not be empty");
    PANIC_IF(list[0].empty(), "Matrix list rows must not be empty");
    num_rows_ = list.size();
    num_cols_ = list[0].size();
    data_.assign(num_rows_ * num_cols_, T());
    int element_index = 0;
    for (int row_index = 0; row_index < num_rows_; ++row_index) {
      const Vector<T>& row = list[row_index];
      PANIC_IF(row.size() != num_cols_, "Matrix row {} has size {}, expected {}", row_index,
               row.size(), num_cols_);
      for (const T& element : row) {
        data_[element_index++] = element;
      }
    }
  }

  [[nodiscard]] auto data() const -> const Vector<T>& { return data_; }
  [[nodiscard]] auto num_rows() const -> int { return num_rows_; }
  [[nodiscard]] auto num_cols() const -> int { return num_cols_; }
  [[nodiscard]] auto num_elements() const -> int { return data_.size(); }

  /**
   * @brief Accesses the element at `[row, col]`.
   */
  auto operator[](int row, int col) -> T& {
    PANIC_IF(row < 0 || row >= num_rows_, "Row index {} out of bounds [0, {}]", row, num_rows_);
    PANIC_IF(col < 0 || col >= num_cols_, "Column index {} out of bounds [0, {}]", col, num_cols_);
    return data_[(row * num_cols_) + col];
  }

  auto operator[](int row, int col) const -> const T& {
    PANIC_IF(row < 0 || row >= num_rows_, "Row index {} out of bounds [0, {}]", row, num_rows_);
    PANIC_IF(col < 0 || col >= num_cols_, "Column index {} out of bounds [0, {}]", col, num_cols_);
    return data_[(row * num_cols_) + col];
  }

  /**
   * @brief Fills the entire matrix with the specified value.
   */
  void fill(T value) { std::ranges::fill(data_, value); }

 private:
  Vector<T> data_;
  int num_rows_;
  int num_cols_;
};

template <typename T>
struct CsrRowView {
  Span<const int> indices;  // NOLINT(misc-non-private-member-variables-in-classes)
  Span<T> values;           // NOLINT(misc-non-private-member-variables-in-classes)

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
  Vector<int> indices_;
  Vector<T> values_;
  Vector<int> offsets_;
  bool has_indices_;

 public:
  /**
   * @brief Constructs a CSR matrix from a dense matrix.
   */
  CsrMatrix(const Matrix<T>& matrix, int expected_size = 0, bool track_indices = true,
            T trash_value = T())
      : has_indices_(track_indices) {
    if (has_indices_ && expected_size > 0) {
      indices_.reserve(expected_size);
    }
    if (expected_size > 0) {
      values_.reserve(expected_size);
    }
    offsets_.assign(matrix.num_rows() + 1, 0);
    int index = 0;
    const Vector<T>& matrix_data = matrix.data();
    for (int i = 0; i < matrix.num_rows(); ++i) {
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
      offsets_[i + 1] = values_.size();
    }
    if (has_indices_) {
      indices_.shrink_to_fit();
    }
    values_.shrink_to_fit();
  }

  /**
   * @brief Constructs a CSR matrix from a 2D Vector (list of lists).
   */
  CsrMatrix(const Vector<Vector<T>>& list, int expected_size = 0, bool track_indices = true,
            T trash_value = T())
      : has_indices_(track_indices) {
    if (has_indices_ && expected_size > 0) {
      indices_.reserve(expected_size);
    }
    if (expected_size > 0) {
      values_.reserve(expected_size);
    }
    offsets_.assign(list.size() + 1, 0);
    for (int i = 0; i < list.size(); ++i) {
      const Vector<T>& row = list[i];
      for (int j = 0; j < row.size(); ++j) {
        const T& value = row[j];
        if (value == trash_value) {
          continue;
        }
        if (has_indices_) {
          indices_.emplace_back(j);
        }
        values_.emplace_back(value);
      }
      offsets_[i + 1] = values_.size();
    }
    if (has_indices_) {
      indices_.shrink_to_fit();
    }
    values_.shrink_to_fit();
  }

  /**
   * @brief Constructs rows from flat values and CSR offsets with empty column-index spans.
   * Offsets must be nondecreasing, start at zero, and end at values.size().
   */
  CsrMatrix(Vector<T> values, Vector<int> row_offsets)
      : values_(std::move(values)), offsets_(std::move(row_offsets)), has_indices_(false) {
    PANIC_IF(offsets_.empty() || offsets_.front() != 0 || offsets_.back() != values_.size() ||
                 !std::ranges::is_sorted(offsets_),
             "Invalid CSR row offsets");
  }

  [[nodiscard]] auto num_rows() const -> int { return offsets_.size() - 1; }
  [[nodiscard]] auto num_elements() const -> int { return values_.size(); }
  [[nodiscard]] auto get_offsets() const -> const Vector<int>& { return offsets_; }

  /**
   * @brief Access a specific row of the matrix.
   */
  auto operator[](int row_index) -> CsrRowView<T> {
    PANIC_IF(row_index < 0 || row_index >= num_rows(), "Row index {} out of bounds [0, {}]",
             row_index, num_rows() - 1);
    const int start = offsets_[row_index];
    const int end = offsets_[row_index + 1];
    return CsrRowView<T>{.indices = has_indices_
                                        ? Span<const int>{indices_}.subspan(start, end - start)
                                        : Span<const int>{},
                         .values = Span<T>{values_}.subspan(start, end - start)};
  }

  auto operator[](int row_index) const -> CsrRowView<const T> {
    PANIC_IF(row_index < 0 || row_index >= num_rows(), "Row index {} out of bounds [0, {}]",
             row_index, num_rows() - 1);
    const int start = offsets_[row_index];
    const int end = offsets_[row_index + 1];
    return CsrRowView<const T>{
        .indices = has_indices_ ? Span<const int>{indices_}.subspan(start, end - start)
                                : Span<const int>{},
        .values = Span<const T>{values_}.subspan(start, end - start)};
  }
};
