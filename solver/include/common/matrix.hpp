#pragma once

#include <cassert>
#include <cstddef>
#include <span>
#include <vector>

namespace common {

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
  int num_rows() const { return rows; }
  int num_cols() const { return cols; }

  /**
   * @brief Initializes a matrix of size r x c with a default value. 
   */
  Matrix(int r, int c, T value = T())
    : rows(r), cols(c)
  {
    assert(r > 0 && "Matrix rows must be positive.");
    assert(c > 0 && "Matrix columns must be positive.");
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

  /**
   * @brief Accesses the element at `(r, c)` by reference (read/write). 
   */
  T& operator()(int r, int c) {
    assert(r < rows && "Row index out-of-bounds.");
    assert(c < cols && "Column index out-of-bounds.");
    return data[r * cols + c];
  }

  /**
   * @brief Accesses the element at `(r, c)` by value (read-only). 
   */
  T operator()(int r, int c) const {
    assert(r < rows && "Row index out-of-bounds.");
    assert(c < cols && "Column index out-of-bounds.");
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

/**
 * @brief A generic Compressed Sparse Row matrix structure.
 * Implements flat 1D vectors for data and offsets to ensure memory continuity and fast, cache-friendly row iteration.
 */
template<typename T>
struct CsrMatrix {
private:
  std::vector<CsrElement<T>> data;
  std::vector<int> offsets;

public:
  /**
   * @brief Default constructor for an empty CSR matrix. 
   */
  CsrMatrix() = default;

  /**
   * @brief Converts a dense 2D Matrix into a CSR format. 
   */
  CsrMatrix(const Matrix<T>& matrix) {
    offsets.assign(matrix.num_rows() + 1, 0);
    for (int i = 0; i < matrix.num_rows(); ++i) {
      for (int j = 0; j < matrix.num_cols(); ++j) {
        T val = matrix(i, j);
        if (i != j && val > 0) {
          data.emplace_back(static_cast<int>(j), val);
        }
      }
      offsets[i + 1] = data.size();
    }
    data.shrink_to_fit();
  }

  /**
   * @brief Access a specific row of the matrix.
   * Usage: for (const auto& element : matrix[i]) { ... }
   */
  std::span<const CsrElement<T>> operator[](int row_index) const {
    assert(row_index + 1 < offsets.size());
    return {
      data.begin() + offsets[row_index],
      data.begin() + offsets[row_index + 1]
    };
  }
};

} // namespace common