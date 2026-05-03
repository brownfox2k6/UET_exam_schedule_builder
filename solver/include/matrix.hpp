#pragma once

#include <cstddef>
#include <vector>
#include <cassert>

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
   * @brief Accesses the element at (r, c) by reference (read/write). 
   */
  inline T& operator()(int r, int c) {
    assert(r < rows && "Row index out-of-bounds.");
    assert(c < cols && "Column index out-of-bounds.");
    return data[r * cols + c];
  }

  /**
   * @brief Accesses the element at (r, c) by value (read-only). 
   */
  inline T operator()(int r, int c) const {
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

} // namespace common