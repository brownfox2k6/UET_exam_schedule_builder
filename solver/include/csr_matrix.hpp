#pragma once

#include <cstddef>
#include <vector>
#include <cassert>
#include "matrix.hpp"

namespace common {

/**
 * @brief A generic Compressed Sparse Row matrix structure.
 * Implements flat 1D vectors for data and offsets to ensure memory continuity and fast, cache-friendly row iteration.
 */
template<typename T>
struct CsrMatrix {
private:
  std::vector<T> columns;
  std::vector<size_t> offsets;

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
    for (size_t i = 0; i < matrix.rows; ++i) {
      for (size_t j = 0; j < matrix.cols; ++j) {
        if (i != j && matrix(i, j) > 0) {
          columns.emplace_back(static_cast<T>(j));
        }
      }
      offsets[i + 1] = columns.size();
    }
    columns.shrink_to_fit();
  }

  /**
   * @brief Returns an iterator pair (begin, end) for the non-zero elements in the specified row. 
   */
  auto row_range(size_t row_index) const {
    return std::make_pair(
      columns.begin() + offsets[row_index],
      columns.begin() + offsets[row_index + 1]
    );
  }
};

} // namespace common