#pragma once

#include <cassert>
#include <concepts>
#include <iterator>
#include <ranges>
#include <type_traits>
#include <utility>

/**
 * Non-owning contiguous view with `int` (`size_type`) sizes, counts, and indices. Targets C++23.
 *
 * Preconditions: the viewed range remains alive and its size fits in `size_type`. Bounds are
 * checked with `assert` in Debug builds.
 */
template <typename T>
class Span {
public:
  using element_type = T;
  using value_type = std::remove_cv_t<T>;
  using size_type = int;
  using difference_type = int;
  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;
  using iterator = pointer;
  using const_iterator = const_pointer;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  constexpr Span() noexcept = default;

  constexpr Span(pointer data, size_type size) noexcept
      : data_(data),
        size_(size) {
    assert(size_ >= 0);
  }

  template <std::contiguous_iterator Iterator, std::sized_sentinel_for<Iterator> Sentinel>
    requires std::convertible_to<std::iter_reference_t<Iterator>, reference>
  constexpr Span(Iterator first, Sentinel last)
      : data_(std::to_address(first)),
        size_(size_type(last - first)) {
    assert(size_ >= 0);
  }

  template <std::ranges::contiguous_range Range>
    requires std::ranges::sized_range<Range>
                 && std::convertible_to<std::ranges::range_reference_t<Range>, reference>
  constexpr Span(Range&& range)
      : data_(std::ranges::data(std::forward<Range>(range))),
        size_(size_type(std::ranges::size(std::forward<Range>(range)))) {}

  template <typename Other>
    requires std::convertible_to<Other*, pointer>
  constexpr Span(const Span<Other>& other) noexcept
      : data_(other.data()),
        size_(other.size()) {}

  [[nodiscard]] constexpr auto data() const noexcept -> pointer { return data_; }
  [[nodiscard]] constexpr auto size() const noexcept -> size_type { return size_; }
  [[nodiscard]] constexpr auto empty() const noexcept -> bool { return size_ == 0; }

  [[nodiscard]] constexpr auto operator[](size_type index) const noexcept -> reference {
    assert(index >= 0 && index < size_);
    return *std::ranges::next(data_, index);
  }

  [[nodiscard]] constexpr auto front() const noexcept -> reference {
    assert(size_ > 0);
    return *data_;
  }

  [[nodiscard]] constexpr auto back() const noexcept -> reference {
    assert(size_ > 0);
    return *std::ranges::next(data_, size_ - 1);
  }

  [[nodiscard]] constexpr auto begin() const noexcept -> iterator { return data_; }
  [[nodiscard]] constexpr auto cbegin() const noexcept -> const_iterator { return data_; }

  [[nodiscard]] constexpr auto end() const noexcept -> iterator {
    return std::ranges::next(data_, size_);
  }

  [[nodiscard]] constexpr auto cend() const noexcept -> const_iterator {
    return std::ranges::next(data_, size_);
  }

  [[nodiscard]] constexpr auto rbegin() const noexcept -> reverse_iterator {
    return reverse_iterator(end());
  }

  [[nodiscard]] constexpr auto crbegin() const noexcept -> const_reverse_iterator {
    return const_reverse_iterator(cend());
  }

  [[nodiscard]] constexpr auto rend() const noexcept -> reverse_iterator {
    return reverse_iterator(begin());
  }

  [[nodiscard]] constexpr auto crend() const noexcept -> const_reverse_iterator {
    return const_reverse_iterator(cbegin());
  }

  [[nodiscard]] constexpr auto first(size_type count) const noexcept -> Span {
    assert(count >= 0 && count <= size_);
    return {data_, count};
  }

  [[nodiscard]] constexpr auto last(size_type count) const noexcept -> Span {
    assert(count >= 0 && count <= size_);
    return {std::ranges::next(data_, size_ - count), count};
  }

  [[nodiscard]] constexpr auto subspan(size_type offset) const noexcept -> Span {
    assert(offset >= 0 && offset <= size_);
    return {std::ranges::next(data_, offset), size_ - offset};
  }

  [[nodiscard]] constexpr auto subspan(size_type offset, size_type count) const noexcept -> Span {
    assert(offset >= 0 && count >= 0 && offset + count <= size_);
    return {std::ranges::next(data_, offset), count};
  }

private:
  pointer data_ = nullptr;
  size_type size_ = 0;
};

template <std::contiguous_iterator Iterator, std::sized_sentinel_for<Iterator> Sentinel>
Span(Iterator, Sentinel) -> Span<std::remove_reference_t<std::iter_reference_t<Iterator>>>;

template <std::ranges::contiguous_range Range>
Span(Range&&) -> Span<std::remove_reference_t<std::ranges::range_reference_t<Range>>>;

template <typename T>
inline constexpr bool std::ranges::enable_borrowed_range<Span<T>> = true;

template <typename T>
inline constexpr bool std::ranges::enable_view<Span<T>> = true;
