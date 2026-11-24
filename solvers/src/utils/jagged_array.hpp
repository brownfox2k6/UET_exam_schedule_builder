#pragma once

#include <cassert>
#include <initializer_list>
#include <utility>

#include "utils/span.hpp"
#include "utils/vector.hpp"

/**
 * Variable-length 2D array stored in flat contiguous memory (`values` + row `offsets`).
 * Provides cache-friendly row iteration via zero-copy `Span` views without nested heap vectors.
 */
template <typename T>
class JaggedArray {
public:
  JaggedArray() = default;

  /** Constructs directly from pre-built flat values and row offsets. */
  JaggedArray(Vector<T> values, Vector<int> offsets)
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

  void append(const T& value) { values_.push_back(value); }
  void append(T&& value) { values_.push_back(std::move(value)); }

  void finish_row() { offsets_.push_back(values_.size()); }

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
  [[nodiscard]] auto offsets() const noexcept -> const Vector<int>& { return offsets_; }

  [[nodiscard]] auto operator[](int row_index) -> Span<T> {
    assert(row_index >= 0 && row_index < num_rows());
    const int start = offsets_[row_index];
    const int end = offsets_[row_index + 1];
    return Span<T>{values_}.subspan(start, end - start);
  }

  [[nodiscard]] auto operator[](int row_index) const -> Span<const T> {
    assert(row_index >= 0 && row_index < num_rows());
    const int start = offsets_[row_index];
    const int end = offsets_[row_index + 1];
    return Span<const T>{values_}.subspan(start, end - start);
  }

private:
  Vector<T> values_;
  Vector<int> offsets_{0};
};
