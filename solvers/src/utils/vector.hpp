#pragma once

#include <algorithm>
#include <cassert>
#include <concepts>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <ranges>
#include <type_traits>
#include <utility>
#include <vector>
#include <version>

/**
 * std::vector wrapper with `int` (`size_type`) sizes, counts, and indices. Targets C++23.
 *
 * Preconditions: sizes and capacities fit in `size_type`; counts are non-negative; indices passed
 * to unchecked operations are in range (verified with `assert` in Debug builds). `at()` retains
 * `std::vector::at()`'s `std::out_of_range` checking.
 * Iterators and their difference types remain those of `std::vector`.
 * Range-container APIs are exposed when the STL supports them
 * (`__cpp_lib_containers_ranges >= 202202L`).
 */
template <typename T, typename Allocator = std::allocator<T>>
class Vector {
public:
  using storage_type = std::vector<T, Allocator>;
  using value_type = typename storage_type::value_type;
  using allocator_type = typename storage_type::allocator_type;
  using size_type = int;
  using difference_type = typename storage_type::difference_type;
  using reference = typename storage_type::reference;
  using const_reference = typename storage_type::const_reference;
  using pointer = typename storage_type::pointer;
  using const_pointer = typename storage_type::const_pointer;
  using iterator = typename storage_type::iterator;
  using const_iterator = typename storage_type::const_iterator;
  using reverse_iterator = typename storage_type::reverse_iterator;
  using const_reverse_iterator = typename storage_type::const_reverse_iterator;

  constexpr Vector() = default;

  constexpr explicit Vector(const allocator_type& allocator) noexcept
      : data_(allocator) {}

  constexpr explicit Vector(size_type count, const allocator_type& allocator = allocator_type())
      : data_(to_storage_size(count), allocator) {}

  constexpr Vector(
      size_type count,
      const T& value,
      const allocator_type& allocator = allocator_type()
  )
      : data_(to_storage_size(count), value, allocator) {}

  template <std::input_iterator InputIterator>
  constexpr Vector(
      InputIterator first,
      InputIterator last,
      const allocator_type& allocator = allocator_type()
  )
      : data_(first, last, allocator) {}

  constexpr Vector(
      std::initializer_list<T> values,
      const allocator_type& allocator = allocator_type()
  )
      : data_(values, allocator) {}

#if defined(__cpp_lib_containers_ranges) && __cpp_lib_containers_ranges >= 202202L
  template <std::ranges::input_range Range>
    requires std::convertible_to<std::ranges::range_reference_t<Range>, T>
  constexpr Vector(
      std::from_range_t from_range,
      Range&& values,
      const allocator_type& allocator = allocator_type()
  )
      : data_(from_range, std::forward<Range>(values), allocator) {}
#endif

  constexpr Vector(const Vector&) = default;
  constexpr Vector(Vector&&) = default;
  constexpr auto operator=(const Vector&) -> Vector& = default;
  constexpr auto operator=(Vector&&) -> Vector& = default;
  constexpr ~Vector() = default;

  constexpr Vector(const Vector& other, const std::type_identity_t<allocator_type>& allocator)
      : data_(other.data_, allocator) {}

  constexpr Vector(Vector&& other, const std::type_identity_t<allocator_type>& allocator)
      : data_(std::move(other).data_, allocator) {}

  constexpr explicit Vector(const storage_type& data)
      : data_(data) {}

  constexpr explicit Vector(
      storage_type&& data
  ) noexcept(std::is_nothrow_move_constructible_v<storage_type>)
      : data_(std::move(data)) {}

  constexpr auto operator=(std::initializer_list<T> values) -> Vector& {
    data_ = values;
    return *this;
  }

  constexpr auto assign(size_type count, const T& value) -> void {
    data_.assign(to_storage_size(count), value);
  }

  template <std::input_iterator InputIterator>
  constexpr auto assign(InputIterator first, InputIterator last) -> void {
    data_.assign(first, last);
  }

  constexpr auto assign(std::initializer_list<T> values) -> void { data_.assign(values); }

#if defined(__cpp_lib_containers_ranges) && __cpp_lib_containers_ranges >= 202202L
  template <std::ranges::input_range Range>
    requires std::convertible_to<std::ranges::range_reference_t<Range>, T>
  constexpr auto assign_range(Range&& values) -> void {
    data_.assign_range(std::forward<Range>(values));
  }

  template <std::ranges::input_range Range>
    requires std::convertible_to<std::ranges::range_reference_t<Range>, T>
  constexpr auto append_range(Range&& values) -> void {
    data_.append_range(std::forward<Range>(values));
  }

  template <std::ranges::input_range Range>
    requires std::convertible_to<std::ranges::range_reference_t<Range>, T>
  constexpr auto insert_range(const_iterator position, Range&& values) -> iterator {
    return data_.insert_range(position, std::forward<Range>(values));
  }
#endif

  [[nodiscard]] constexpr auto get_allocator() const noexcept -> allocator_type {
    return data_.get_allocator();
  }

  [[nodiscard]] constexpr auto size() const noexcept -> size_type {
    return size_type(data_.size());
  }

  [[nodiscard]] constexpr auto capacity() const noexcept -> size_type {
    return size_type(data_.capacity());
  }

  [[nodiscard]] constexpr auto max_size() const noexcept -> size_type {
    // Report the wrapper's `size_type` limit without adding growth-time checks.
    constexpr auto limit = storage_size_type(std::numeric_limits<size_type>::max());
    return size_type(std::min(data_.max_size(), limit));
  }

  [[nodiscard]] constexpr auto empty() const noexcept -> bool { return data_.empty(); }

  constexpr auto reserve(size_type count) -> void { data_.reserve(to_storage_size(count)); }

  constexpr auto shrink_to_fit() -> void { data_.shrink_to_fit(); }

  [[nodiscard]] constexpr auto operator[](size_type index) noexcept -> reference {
    assert(index >= 0 && index < size());
    return data_[storage_size_type(index)];
  }

  [[nodiscard]] constexpr auto operator[](size_type index) const noexcept -> const_reference {
    assert(index >= 0 && index < size());
    return data_[storage_size_type(index)];
  }

  [[nodiscard]] constexpr auto at(size_type index) -> reference {
    return data_.at(storage_size_type(index));
  }

  [[nodiscard]] constexpr auto at(size_type index) const -> const_reference {
    return data_.at(storage_size_type(index));
  }

  [[nodiscard]] constexpr auto front() noexcept -> reference {
    assert(!empty());
    return data_.front();
  }

  [[nodiscard]] constexpr auto front() const noexcept -> const_reference {
    assert(!empty());
    return data_.front();
  }

  [[nodiscard]] constexpr auto back() noexcept -> reference {
    assert(!empty());
    return data_.back();
  }

  [[nodiscard]] constexpr auto back() const noexcept -> const_reference {
    assert(!empty());
    return data_.back();
  }

  [[nodiscard]] constexpr auto data() noexcept -> pointer
    requires(!std::same_as<T, bool>)
  {
    return data_.data();
  }

  [[nodiscard]] constexpr auto data() const noexcept -> const_pointer
    requires(!std::same_as<T, bool>)
  {
    return data_.data();
  }

  [[nodiscard]] constexpr auto begin() noexcept -> iterator { return data_.begin(); }
  [[nodiscard]] constexpr auto begin() const noexcept -> const_iterator { return data_.begin(); }
  [[nodiscard]] constexpr auto cbegin() const noexcept -> const_iterator { return data_.cbegin(); }

  [[nodiscard]] constexpr auto end() noexcept -> iterator { return data_.end(); }
  [[nodiscard]] constexpr auto end() const noexcept -> const_iterator { return data_.end(); }
  [[nodiscard]] constexpr auto cend() const noexcept -> const_iterator { return data_.cend(); }

  [[nodiscard]] constexpr auto rbegin() noexcept -> reverse_iterator { return data_.rbegin(); }
  [[nodiscard]] constexpr auto rbegin() const noexcept -> const_reverse_iterator {
    return data_.rbegin();
  }
  [[nodiscard]] constexpr auto crbegin() const noexcept -> const_reverse_iterator {
    return data_.crbegin();
  }

  [[nodiscard]] constexpr auto rend() noexcept -> reverse_iterator { return data_.rend(); }
  [[nodiscard]] constexpr auto rend() const noexcept -> const_reverse_iterator {
    return data_.rend();
  }
  [[nodiscard]] constexpr auto crend() const noexcept -> const_reverse_iterator {
    return data_.crend();
  }

  constexpr auto clear() noexcept -> void { data_.clear(); }

  constexpr auto insert(const_iterator position, const T& value) -> iterator {
    return data_.insert(position, value);
  }

  constexpr auto insert(const_iterator position, T&& value) -> iterator {
    return data_.insert(position, std::move(value));
  }

  constexpr auto insert(const_iterator position, size_type count, const T& value) -> iterator {
    return data_.insert(position, to_storage_size(count), value);
  }

  template <std::input_iterator InputIterator>
  constexpr auto insert(const_iterator position, InputIterator first, InputIterator last)
      -> iterator {
    return data_.insert(position, first, last);
  }

  constexpr auto insert(const_iterator position, std::initializer_list<T> values) -> iterator {
    return data_.insert(position, values);
  }

  template <typename... Args>
  constexpr auto emplace(const_iterator position, Args&&... args) -> iterator {
    return data_.emplace(position, std::forward<Args>(args)...);
  }

  constexpr auto erase(const_iterator position) -> iterator { return data_.erase(position); }

  constexpr auto erase(const_iterator first, const_iterator last) -> iterator {
    return data_.erase(first, last);
  }

  constexpr auto push_back(const T& value) -> void { data_.push_back(value); }
  constexpr auto push_back(T&& value) -> void { data_.push_back(std::move(value)); }

  template <typename... Args>
  constexpr auto emplace_back(Args&&... args) -> reference {
    return data_.emplace_back(std::forward<Args>(args)...);
  }

  constexpr auto pop_back() -> void {
    assert(!empty());
    data_.pop_back();
  }

  constexpr auto resize(size_type count) -> void { data_.resize(to_storage_size(count)); }

  constexpr auto resize(size_type count, const T& value) -> void {
    data_.resize(to_storage_size(count), value);
  }

  constexpr auto swap(Vector& other) noexcept(noexcept(data_.swap(other.data_))) -> void {
    data_.swap(other.data_);
  }

  constexpr auto flip() noexcept -> void
    requires std::same_as<T, bool>
  {
    data_.flip();
  }

  // Explicit access for APIs that specifically require `std::vector`.
  [[nodiscard]] constexpr auto std_vector() noexcept -> storage_type& { return data_; }
  [[nodiscard]] constexpr auto std_vector() const noexcept -> const storage_type& { return data_; }

  friend constexpr auto operator==(const Vector&, const Vector&) -> bool = default;

  friend constexpr auto operator<=>(const Vector& left, const Vector& right) -> decltype(auto)
    requires requires(const storage_type& storage) { storage <=> storage; }
  {
    return left.data_ <=> right.data_;
  }

private:
  using storage_size_type = typename storage_type::size_type;
  storage_type data_;

  [[nodiscard]] static constexpr auto to_storage_size(size_type count) noexcept
      -> storage_size_type {
    assert(count >= 0);
    return storage_size_type(count);
  }
};

template <
    std::input_iterator InputIterator,
    typename T = typename std::iterator_traits<InputIterator>::value_type,
    typename Allocator = std::allocator<T>>
Vector(InputIterator, InputIterator, Allocator = Allocator()) -> Vector<T, Allocator>;

#if defined(__cpp_lib_containers_ranges) && __cpp_lib_containers_ranges >= 202202L
template <
    std::ranges::input_range Range,
    typename Allocator = std::allocator<std::ranges::range_value_t<Range>>>
Vector(std::from_range_t from_range, Range&& values, Allocator allocator = Allocator())
    -> Vector<std::ranges::range_value_t<Range>, Allocator>;
#endif

template <typename T, typename Allocator>
constexpr auto swap(Vector<T, Allocator>& left, Vector<T, Allocator>& right) noexcept(
    noexcept(left.swap(right))
) -> void {
  left.swap(right);
}

// ADL equivalents of `std::erase` / `std::erase_if`, returning `Vector::size_type`.
template <typename T, typename Allocator, typename Value>
constexpr auto erase(Vector<T, Allocator>& values, const Value& value) ->
    typename Vector<T, Allocator>::size_type {
  return typename Vector<T, Allocator>::size_type(std::erase(values.std_vector(), value));
}

template <typename T, typename Allocator, typename Predicate>
constexpr auto erase_if(Vector<T, Allocator>& values, Predicate predicate) ->
    typename Vector<T, Allocator>::size_type {
  return typename Vector<T, Allocator>::size_type(
      std::erase_if(values.std_vector(), std::move(predicate))
  );
}
