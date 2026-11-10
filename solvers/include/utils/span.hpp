#pragma once

#include <concepts>
#include <iterator>
#include <ranges>
#include <type_traits>
#include <utility>

/**
 * std::span wrapper with int sizes, counts, and indices. Targets C++23.
 *
 * Preconditions: the viewed range remains alive and its size fits in int.
 */
template <typename T>
class Span {
 public:
  using element_type = T;
  using value_type = std::remove_cv_t<T>;
  using size_type = int;
  using difference_type = int;
  using pointer = T*;
  using reference = T&;
  using iterator = pointer;
  using const_iterator = const value_type*;

 private:
  pointer data_ = nullptr;
  int size_ = 0;

 public:
  constexpr Span() noexcept = default;

  constexpr Span(pointer data, int size) noexcept : data_(data), size_(size) {}

  template <std::contiguous_iterator It, std::sized_sentinel_for<It> Sentinel>
    requires std::convertible_to<std::iter_reference_t<It>, reference>
  constexpr Span(It first, Sentinel last)
      : data_(std::to_address(first)), size_(static_cast<int>(last - first)) {}

  template <std::ranges::contiguous_range R>
    requires std::ranges::sized_range<R> &&
                 std::convertible_to<std::ranges::range_reference_t<R>, reference>
  constexpr Span(R&& range)
      : data_(std::ranges::data(std::forward<R>(range))),
        size_(static_cast<int>(std::ranges::size(std::forward<R>(range)))) {}

  template <typename U>
    requires std::convertible_to<U*, pointer>
  constexpr Span(const Span<U>& other) noexcept : data_(other.data()), size_(other.size()) {}

  [[nodiscard]] constexpr auto data() const noexcept -> pointer { return data_; }
  [[nodiscard]] constexpr auto size() const noexcept -> int { return size_; }
  [[nodiscard]] constexpr auto empty() const noexcept -> bool { return size_ == 0; }

  [[nodiscard]] constexpr auto operator[](int index) const noexcept -> reference {
    return *std::ranges::next(data_, index);
  }

  [[nodiscard]] constexpr auto begin() const noexcept -> iterator { return data_; }
  [[nodiscard]] constexpr auto end() const noexcept -> iterator {
    return std::ranges::next(data_, size_);
  }

  [[nodiscard]] constexpr auto subspan(int offset, int count) const noexcept -> Span {
    return {std::ranges::next(data_, offset), count};
  }
};
