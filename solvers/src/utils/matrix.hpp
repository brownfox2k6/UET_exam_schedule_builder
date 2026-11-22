#pragma once

#include <algorithm>
#include <cassert>

#include "utils/span.hpp"
#include "utils/vector.hpp"

/**
 * @brief A generic 2D matrix structure.
 * Implements a flat 1D vector back-end to ensure memory continuity.
 */
template <typename T>
class Matrix {
public:
  Matrix() = default;

  /**
   * @brief Initializes a matrix of size r x c with a default value.
   */
  Matrix(int rows, int cols, T value = T())
      : num_rows_(rows),
        num_cols_(cols) {
    assert(rows > 0 && cols > 0);
    data_.assign(num_rows_ * num_cols_, value);
  }

  /**
   * @brief Constructs a matrix from a 2D Vector (list of lists).
   */
  Matrix(const Vector<Vector<T>>& list)
      : num_rows_(list.size()),
        num_cols_(list.empty() ? 0 : list.front().size()) {
    assert(num_rows_ > 0 && num_cols_ > 0);
    data_.assign(num_rows_ * num_cols_, T());
    int element_index = 0;
    for (int row_index = 0; row_index < num_rows_; ++row_index) {
      const Vector<T>& row = list[row_index];
      assert(row.size() == num_cols_);
      for (const T& element : row) {
        data_[element_index++] = element;
      }
    }
  }

  [[nodiscard]] auto data() const noexcept -> const Vector<T>& { return data_; }
  [[nodiscard]] auto num_rows() const noexcept -> int { return num_rows_; }
  [[nodiscard]] auto num_cols() const noexcept -> int { return num_cols_; }
  [[nodiscard]] auto num_elements() const noexcept -> int { return data_.size(); }

  /**
   * @brief Accesses the element at `[row, col]`.
   */
  auto operator[](int row, int col) -> T& {
    assert(row >= 0 && row < num_rows_);
    assert(col >= 0 && col < num_cols_);
    return data_[(row * num_cols_) + col];
  }

  auto operator[](int row, int col) const -> const T& {
    assert(row >= 0 && row < num_rows_);
    assert(col >= 0 && col < num_cols_);
    return data_[(row * num_cols_) + col];
  }

  /**
   * @brief Fills the entire matrix with the specified value.
   */
  void fill(T value) { std::ranges::fill(data_, value); }

private:
  Vector<T> data_;
  int num_rows_ = 0;
  int num_cols_ = 0;
};

template <typename T>
struct CsrRowView {
  Span<const int> indices;  // NOLINT(misc-non-private-member-variables-in-classes)
  Span<T> values;           // NOLINT(misc-non-private-member-variables-in-classes)

  [[nodiscard]] auto size() const noexcept -> int { return int(values.size()); }
};
