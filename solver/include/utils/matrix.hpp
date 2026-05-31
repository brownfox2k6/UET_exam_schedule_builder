#pragma once

#include "utils/assert.hpp"

#include <cassert>
#include <cstddef>
#include <span>
#include <vector>

namespace utils {

/**
* @brief A generic 2D matrix structure.
* Implements a flat 1D vector back-end to ensure memory continuity. 
*/
template<typename T>
struct Matrix {
private:
  std::vector<T> data;
  int rows;
  int cols;

public:
  /**
   * @brief Initializes a matrix of size r x c with a default value. 
   */
  Matrix(int r, int c, T value = T()) : rows(r), cols(c) {
    PANIC_IF(r <= 0, "Matrix rows must be positive (got: {})", rows);
    PANIC_IF(c <= 0, "Matrix columns must be positive (got: {})", cols);
    data.assign(rows * cols, value);
  }

  /**
   * @brief Constructs a matrix from a 2D std::vector (list of lists). 
   */
  Matrix(const std::vector<std::vector<T>>& list) {
    PANIC_IF(list.empty(), "Matrix list must not be empty");
    PANIC_IF(list[0].empty(), "Matrix list rows must not be empty");
    rows = list.size();
    cols = list[0].size();
    data.assign(rows * cols, T());
    for (int r = 0; r < rows; ++r) {
      PANIC_IF(
        static_cast<int>(list[r].size()) != cols,
        "Matrix row {} has size {}, expected {}", r, list[r].size(), cols
      );
      for (int c = 0; c < cols; ++c) {
        data[r * cols + c] = list[r][c];
      }
    }
  }

  int num_rows() const { return rows; }
  int num_cols() const { return cols; }
  int num_elements() const { return data.size(); }

  /**
   * @brief Accesses the element at `(r, c)` by reference (read/write). 
   */
  T& operator()(int r, int c) {
    PANIC_IF(r < 0 || r >= rows, "Row index {} out of bounds [0, {}]", r, rows);
    PANIC_IF(c < 0 || c >= cols, "Column index {} out of bounds [0, {}]", c, cols);
    return data[r * cols + c];
  }

  /**
   * @brief Accesses the element at `(r, c)` by value (read-only). 
   */
  const T& operator()(int r, int c) const {
    PANIC_IF(r < 0 || r >= rows, "Row index {} out of bounds [0, {}]", r, rows);
    PANIC_IF(c < 0 || c >= cols, "Column index {} out of bounds [0, {}]", c, cols);
    return data[r * cols + c];
  }

  /**
   * @brief Fills the entire matrix with the specified value. 
   */
  void fill(T value) {
    std::fill(data.begin(), data.end(), value);
  }
};

template<typename T>
struct CsrRowConstView {
  std::span<const int> indices;
  std::span<const T> values;

  int size() const { return static_cast<int>(values.size()); }
};

template<typename T>
struct CsrRowView {
  std::span<const int> indices;
  std::span<T> values;

  int size() const { return static_cast<int>(values.size()); }
};

/**
 * @brief A generic Compressed Sparse Row matrix structure.
 * Implements flat 1D vectors for data and offsets to ensure memory continuity and fast, cache-friendly row iteration.
 */
template<typename T>
struct CsrMatrix {
private:
  std::vector<int> indices;
  std::vector<T> values;
  std::vector<int> offsets;
  bool has_indices;

public:
  /**
   * @brief Constructs a CSR matrix from a dense matrix.
   */
  CsrMatrix(
    const Matrix<T>& matrix,
    int expected_size = 0,
    bool track_indices = true,
    T trash_value = 0
  ) {
    has_indices = track_indices;
    if (has_indices) {
      indices.reserve(expected_size);
    }
    values.reserve(expected_size);
    offsets.assign(matrix.num_rows() + 1, 0);
    for (int i = 0; i < matrix.num_rows(); ++i) {
      for (int j = 0; j < matrix.num_cols(); ++j) {
        T value = matrix(i, j);
        if (value > trash_value) {
          if (has_indices) {
            indices.emplace_back(j);
          }
          values.emplace_back(value);
        }
      }
      offsets[i + 1] = values.size();
    }
    if (has_indices) {
      indices.shrink_to_fit();
    }
    values.shrink_to_fit();
  }

  /**
   * @brief Constructs a CSR matrix from a 2D std::vector (list of lists).
   */
  CsrMatrix(
    const std::vector<std::vector<T>>& v,
    int expected_size = 0,
    bool track_indices = true
  ) {
    has_indices = track_indices;
    if (has_indices) {
      indices.reserve(expected_size);
    }
    values.reserve(expected_size);
    offsets.assign(v.size() + 1, 0);
    for (int i = 0; i < v.size(); ++i) {
      for (int j = 0; j < v[i].size(); ++j) {
        if (has_indices) {
          indices.emplace_back(j);
        }
        values.emplace_back(v[i][j]);
      }
      offsets[i + 1] = values.size();
    }
    if (has_indices) {
      indices.shrink_to_fit();
    }
    values.shrink_to_fit();
  }

  int num_rows() const { return offsets.size() - 1; }
  int num_elements() const { return values.size(); }
  const std::vector<int>& get_offsets() const { return offsets; }

  /**
   * @brief Access a specific row of the matrix (read-only).
   * Usage: for (const auto& element : matrix[i]) { ... }
   */
  const CsrRowConstView<T> operator[](int row_index) const {
    PANIC_IF(
      row_index < 0 || row_index >= num_rows(),
      "Row index {} out of bounds [0, {}]", row_index, num_rows() - 1
    );
    const int start = offsets[row_index];
    const int end   = offsets[row_index + 1];
    return CsrRowConstView<T> {
      .indices = has_indices
        ? std::span<const int>{ indices.begin() + start, indices.begin() + end }
        : std::span<const int>{},
      .values = { values.begin() + start, values.begin() + end }
    };
  }

  /**
   * @brief Access a specific row of the matrix (read-write).
   */
  CsrRowView<T> operator[](int row_index) {
    PANIC_IF(
      row_index < 0 || row_index >= num_rows(),
      "Row index {} out of bounds [0, {}]", row_index, num_rows() - 1
    );
    const int start = offsets[row_index];
    const int end   = offsets[row_index + 1];
    return CsrRowView<T> {
      .indices = has_indices
        ? std::span<const int>{ indices.begin() + start, indices.begin() + end }
        : std::span<const int>{},
      .values = { values.begin() + start, values.begin() + end }
    };
  }
};

} // namespace utils