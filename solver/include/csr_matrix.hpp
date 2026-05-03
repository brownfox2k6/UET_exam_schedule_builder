#pragma once

#include <cstddef>
#include <vector>
#include <cassert>
#include <span>
#include "matrix.hpp"

namespace common {

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
    offsets.assign(matrix.rows + 1, 0);
    for (int i = 0; i < matrix.rows; ++i) {
      for (int j = 0; j < matrix.cols; ++j) {
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