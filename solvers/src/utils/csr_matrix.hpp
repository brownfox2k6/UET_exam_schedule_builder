#pragma once

#include <cassert>

#include "utils/matrix.hpp"
#include "utils/span.hpp"
#include "utils/vector.hpp"

template <typename T>
struct CsrRowView {
  // NOLINTBEGIN(*-non-private-*)
  Span<const int> indices;
  Span<T> values;
  // NOLINTEND(*-non-private-*)

  [[nodiscard]] auto size() const noexcept -> int { return values.size(); }
};

/**
 * Compressed Sparse Row (CSR) matrix storing non-zero column indices and values in flat vectors
 * for cache-friendly sparse row iteration.
 */
template <typename T>
class CsrMatrix {
public:
  CsrMatrix() = default;

  /** Constructs a CSR matrix by compressing all entries not equal to `zero_value`. */
  explicit CsrMatrix(const Matrix<T>& matrix, T zero_value = T()) {
    offsets_.assign(matrix.num_rows() + 1, 0);
    const Vector<T>& matrix_data = matrix.data();
    int flat_index = 0;
    for (int row = 0; row < matrix.num_rows(); ++row) {
      for (int col = 0; col < matrix.num_cols(); ++col) {
        const T& value = matrix_data[flat_index++];
        if (value == zero_value) {
          continue;
        }
        indices_.push_back(col);
        values_.push_back(value);
      }
      offsets_[row + 1] = values_.size();
    }
    indices_.shrink_to_fit();
    values_.shrink_to_fit();
  }

  [[nodiscard]] auto num_rows() const noexcept -> int { return offsets_.size() - 1; }
  [[nodiscard]] auto num_elements() const noexcept -> int { return values_.size(); }
  [[nodiscard]] auto offsets() const noexcept -> const Vector<int>& { return offsets_; }

  [[nodiscard]] auto operator[](int row_index) -> CsrRowView<T> {
    assert(row_index >= 0 && row_index < num_rows());
    const int start = offsets_[row_index];
    const int end = offsets_[row_index + 1];
    return {
        .indices = Span<const int>{indices_}.subspan(start, end - start),
        .values = Span<T>{values_}.subspan(start, end - start)
    };
  }

  [[nodiscard]] auto operator[](int row_index) const -> CsrRowView<const T> {
    assert(row_index >= 0 && row_index < num_rows());
    const int start = offsets_[row_index];
    const int end = offsets_[row_index + 1];
    return {
        .indices = Span<const int>{indices_}.subspan(start, end - start),
        .values = Span<const T>{values_}.subspan(start, end - start)
    };
  }

private:
  Vector<int> indices_;
  Vector<T> values_;
  Vector<int> offsets_{0};
};
