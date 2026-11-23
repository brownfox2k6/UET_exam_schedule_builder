#pragma once

#include <cassert>
#include <initializer_list>
#include <utility>

#include "utils/matrix.hpp"
#include "utils/span.hpp"
#include "utils/vector.hpp"

template <typename T>
struct CsrRowView {
  Span<const int> indices;  // NOLINT(misc-non-private-member-variables-in-classes)
  Span<T> values;           // NOLINT(misc-non-private-member-variables-in-classes)

  [[nodiscard]] auto size() const noexcept -> int { return values.size(); }
};

/**
 * @brief A generic Compressed Sparse Row matrix structure.
 * Implements flat 1D vectors for data and offsets to ensure memory continuity and fast,
 * cache-friendly row iteration.
 */
template <typename T>
class CsrMatrix {
public:
  CsrMatrix() = default;

  /**
   * @brief Constructs a CSR matrix from a dense matrix.
   */
  CsrMatrix(
      const Matrix<T>& matrix,
      int expected_size = 0,
      bool track_indices = true,
      T trash_value = T()
  )
      : has_indices_(track_indices) {
    if (has_indices_) {
      indices_.reserve(expected_size);
    }
    values_.reserve(expected_size);
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
          indices_.push_back(j);
        }
        values_.push_back(value);
      }
      offsets_[i + 1] = values_.size();
    }
    if (has_indices_) {
      indices_.shrink_to_fit();
    }
    values_.shrink_to_fit();
  }

  /**
   * @brief Constructs a values-only CSR matrix directly from pre-built offsets and values.
   */
  CsrMatrix(Vector<T> values, Vector<int> offsets)
      : values_(std::move(values)),
        offsets_(std::move(offsets)) {
    assert(!offsets_.empty());
    assert(offsets_.front() == 0);
    assert(offsets_.back() == values_.size());
  }

  void reserve_num_rows(int num_rows) {
    assert(num_rows >= 0);
    offsets_.reserve(num_rows + 1);
  }

  void reserve_num_elements(int num_elements) {
    assert(num_elements >= 0);
    values_.reserve(num_elements);
  }

  void append(const T& value) {
    assert(!has_indices_);
    values_.push_back(value);
  }

  void append(T&& value) {
    assert(!has_indices_);
    values_.push_back(std::move(value));
  }

  void finish_row() {
    assert(!has_indices_);
    offsets_.push_back(values_.size());
  }

  void append_row(std::initializer_list<T> row) {
    for (const T& value : row) {
      append(value);
    }
    finish_row();
  }

  void append_row(Span<const T> row) {
    for (const T& value : row) {
      append(value);
    }
    finish_row();
  }

  void append_row(const Vector<T>& row) {
    for (const T& value : row) {
      append(value);
    }
    finish_row();
  }

  void append_row(Vector<T>&& row) {
    Vector<T> moved_row = std::move(row);
    for (T& value : moved_row) {
      append(std::move(value));
    }
    finish_row();
  }

  [[nodiscard]] auto num_rows() const noexcept -> int { return offsets_.size() - 1; }
  [[nodiscard]] auto num_elements() const noexcept -> int { return values_.size(); }
  [[nodiscard]] auto get_offsets() const noexcept -> const Vector<int>& { return offsets_; }

  /**
   * @brief Access a specific row of the matrix.
   */
  auto operator[](int row_index) -> CsrRowView<T> {
    assert(row_index >= 0 && row_index < num_rows());
    const int start = offsets_[row_index];
    const int end = offsets_[row_index + 1];
    return CsrRowView<T>{
        .indices = has_indices_ ? Span<const int>{indices_}.subspan(start, end - start)
                                : Span<const int>{},
        .values = Span<T>{values_}.subspan(start, end - start)
    };
  }

  auto operator[](int row_index) const -> CsrRowView<const T> {
    assert(row_index >= 0 && row_index < num_rows());
    const int start = offsets_[row_index];
    const int end = offsets_[row_index + 1];
    return CsrRowView<const T>{
        .indices = has_indices_ ? Span<const int>{indices_}.subspan(start, end - start)
                                : Span<const int>{},
        .values = Span<const T>{values_}.subspan(start, end - start)
    };
  }

private:
  Vector<int> indices_;
  Vector<T> values_;
  Vector<int> offsets_{0};
  bool has_indices_ = false;
};
