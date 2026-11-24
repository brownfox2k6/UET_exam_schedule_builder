#pragma once

#include <algorithm>
#include <cassert>

#include "utils/vector.hpp"

/** Generic 2D dense matrix backed by a flat contiguous 1D vector. */
template <typename T>
class Matrix {
public:
  Matrix() = default;

  /** Initializes a matrix of size `rows` x `cols` with a default value. */
  Matrix(int rows, int cols, T value = T())
      : num_rows_(rows),
        num_cols_(cols) {
    assert(rows > 0 && cols > 0);
    data_.assign(num_rows_ * num_cols_, value);
  }

  [[nodiscard]] auto data() const noexcept -> const Vector<T>& { return data_; }
  [[nodiscard]] auto num_rows() const noexcept -> int { return num_rows_; }
  [[nodiscard]] auto num_cols() const noexcept -> int { return num_cols_; }
  [[nodiscard]] auto num_elements() const noexcept -> int { return data_.size(); }

  /** Accesses the element at `[row, col]`. */
  [[nodiscard]] auto operator[](int row, int col) -> T& {
    assert(row >= 0 && row < num_rows_);
    assert(col >= 0 && col < num_cols_);
    return data_[(row * num_cols_) + col];
  }

  [[nodiscard]] auto operator[](int row, int col) const -> const T& {
    assert(row >= 0 && row < num_rows_);
    assert(col >= 0 && col < num_cols_);
    return data_[(row * num_cols_) + col];
  }

  /** Fills the entire matrix with `value`. */
  void fill(T value) { std::ranges::fill(data_, value); }

private:
  Vector<T> data_;
  int num_rows_ = 0;
  int num_cols_ = 0;
};
