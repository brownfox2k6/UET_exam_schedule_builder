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
  Matrix(int r, int c, T value = T())
    : rows(r), cols(c)
  {
    utils::panic_if(r <= 0, "Matrix rows must be positive (got: {})", rows);
    utils::panic_if(c <= 0, "Matrix columns must be positive (got: {})", cols);
    data.assign(rows * cols, value);
  }

  /**
   * @brief Constructs a matrix from a 2D std::vector (list of lists). 
   */
  Matrix(const std::vector<std::vector<T>>& list)
    : Matrix(list.size(), list[0].size())
  {
    for (int r = 0; r < rows; ++r) {
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
    utils::panic_if(r < 0 || r >= rows, "Row index out-of-bounds (got: {}, rows={})", r, rows);
    utils::panic_if(c < 0 || c >= cols, "Column index out-of-bounds (got: {}, cols={})", c, cols);
    return data[r * cols + c];
  }

  /**
   * @brief Accesses the element at `(r, c)` by value (read-only). 
   */
  const T& operator()(int r, int c) const {
    utils::panic_if(r < 0 || r >= rows, "Row index out-of-bounds (got: {}, rows={})", r, rows);
    utils::panic_if(c < 0 || c >= cols, "Column index out-of-bounds (got: {}, cols={})", c, cols);
    return data[r * cols + c];
  }

  /**
   * @brief Fills the entire matrix with the specified value. 
   */
  void fill(T value) {
    std::fill(data.begin(), data.end(), value);
  }
};

/**
 * @brief Represents a non-zero entry in a sparse matrix row.
 * This structure pairs a column index with its corresponding value.
 * Using a struct here ensures that the index and value are stored adjacently in memory, maximizing cache hits during row traversal.
 */
template<typename T>
struct CsrElement {
  int index;
  T value;

  CsrElement(int i, T v) : index(i), value(v) {}

  // For std::accumulate
  friend T operator+(const T& sum, const CsrElement& element) {
    return sum + element.value;
  }
};

template<typename T>
struct CsrRowView {
  std::span<const int> indices;
  std::span<const T> values;

  int size() const {
    return indices.size();
  }
};

/**
 * @brief A generic Compressed Sparse Row matrix structure.
 * Implements flat 1D vectors for data and offsets to ensure memory continuity and fast, cache-friendly row iteration.
 */
template<typename T>
struct CsrMatrix {
private:
  // std::vector<CsrElement<T>> data;
  std::vector<int> indices;
  std::vector<T> values;
  std::vector<int> offsets;
  bool has_indices;

public:
  /**
   * @brief Converts a dense 2D Matrix into a CSR format.
   */
  CsrMatrix(
    const Matrix<T>& matrix,
    int expected_size = 0,
    T trash_value = 0,
    bool track_indices = true
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

  int num_rows() const { return offsets.size() - 1; }

  /**
   * @brief Access a specific row of the matrix.
   * Usage: for (const auto& element : matrix[i]) { ... }
   */
  CsrRowView<T> operator[](int row_index) const {
    utils::panic_if(row_index >= num_rows(),
                    "Row index out-of-bounds (got: {}, rows={})", row_index, num_rows());
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