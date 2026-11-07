#pragma once

#include <algorithm>
#include <concepts>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <ranges>
#include <type_traits>
#include <utility>
#include <vector>

/**
 * std::vector wrapper with int sizes, counts, and indices. Targets C++23.
 *
 * Preconditions: sizes/capacities fit in int; counts are nonnegative; indices
 * passed to unchecked operations are valid. No extra checks are performed.
 * at() retains std::vector::at()'s checked behavior.
 * Iterators and their difference types remain those of std::vector.
 * Range-container APIs are exposed when the STL supports them
 * (__cpp_lib_containers_ranges >= 202202L).
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

 private:
  using storage_size_type = typename storage_type::size_type;
  storage_type data_;

  template <std::integral Integer>
  [[nodiscard]] static constexpr auto to_storage_size(Integer value) noexcept -> storage_size_type {
    return static_cast<storage_size_type>(value);
  }

 public:
  constexpr Vector() = default;

  constexpr explicit Vector(const allocator_type& alloc) noexcept : data_(alloc) {}

  template <std::integral Integer>
  constexpr explicit Vector(Integer count, const allocator_type& alloc = allocator_type())
      : data_(to_storage_size(count), alloc) {}

  template <std::integral Integer>
  constexpr Vector(Integer count, const T& value, const allocator_type& alloc = allocator_type())
      : data_(to_storage_size(count), value, alloc) {}

  template <std::input_iterator InputIt>
  constexpr Vector(InputIt first, InputIt last, const allocator_type& alloc = allocator_type())
      : data_(first, last, alloc) {}

  constexpr Vector(std::initializer_list<T> values, const allocator_type& alloc = allocator_type())
      : data_(values, alloc) {}

#if defined(__cpp_lib_containers_ranges) && __cpp_lib_containers_ranges >= 202202L
  template <std::ranges::input_range R>
    requires std::convertible_to<std::ranges::range_reference_t<R>, T>
  constexpr Vector(std::from_range_t from_range, R&& values,
                   const allocator_type& alloc = allocator_type())
      : data_(from_range, std::forward<R>(values), alloc) {}
#endif

  constexpr Vector(const Vector&) = default;
  constexpr Vector(Vector&&) = default;
  constexpr auto operator=(const Vector&) -> Vector& = default;
  constexpr auto operator=(Vector&&) -> Vector& = default;
  constexpr ~Vector() = default;

  constexpr Vector(const Vector& other, const std::type_identity_t<allocator_type>& alloc)
      : data_(other.data_, alloc) {}

  constexpr Vector(Vector&& other, const std::type_identity_t<allocator_type>& alloc)
      : data_(std::move(other).data_, alloc) {}

  constexpr explicit Vector(const storage_type& data) : data_(data) {}

  constexpr explicit Vector(storage_type&& data) noexcept(
      std::is_nothrow_move_constructible_v<storage_type>)
      : data_(std::move(data)) {}

  constexpr auto operator=(std::initializer_list<T> values) -> Vector& {
    data_ = values;
    return *this;
  }

  template <std::integral Integer>
  constexpr auto assign(Integer count, const T& value) -> void {
    data_.assign(to_storage_size(count), value);
  }

  template <std::input_iterator InputIt>
  constexpr auto assign(InputIt first, InputIt last) -> void {
    data_.assign(first, last);
  }

  constexpr auto assign(std::initializer_list<T> values) -> void { data_.assign(values); }

#if defined(__cpp_lib_containers_ranges) && __cpp_lib_containers_ranges >= 202202L
  template <std::ranges::input_range R>
    requires std::convertible_to<std::ranges::range_reference_t<R>, T>
  constexpr auto assign_range(R&& values) -> void {
    data_.assign_range(std::forward<R>(values));
  }

  template <std::ranges::input_range R>
    requires std::convertible_to<std::ranges::range_reference_t<R>, T>
  constexpr auto append_range(R&& values) -> void {
    data_.append_range(std::forward<R>(values));
  }

  template <std::ranges::input_range R>
    requires std::convertible_to<std::ranges::range_reference_t<R>, T>
  constexpr auto insert_range(const_iterator pos, R&& values) -> iterator {
    return data_.insert_range(pos, std::forward<R>(values));
  }
#endif

  [[nodiscard]] constexpr auto get_allocator() const noexcept -> allocator_type {
    return data_.get_allocator();
  }

  [[nodiscard]] constexpr auto size() const noexcept -> int {
    return static_cast<int>(data_.size());
  }

  [[nodiscard]] constexpr auto capacity() const noexcept -> int {
    return static_cast<int>(data_.capacity());
  }

  [[nodiscard]] constexpr auto max_size() const noexcept -> int {
    // Report the wrapper's int limit, without adding growth-time checks.
    constexpr auto limit = static_cast<storage_size_type>(std::numeric_limits<int>::max());
    return static_cast<int>(std::min(data_.max_size(), limit));
  }

  [[nodiscard]] constexpr auto empty() const noexcept -> bool { return data_.empty(); }

  template <std::integral Integer>
  constexpr auto reserve(Integer count) -> void {
    data_.reserve(to_storage_size(count));
  }

  constexpr auto shrink_to_fit() -> void { data_.shrink_to_fit(); }

  template <std::integral Integer>
  [[nodiscard]] constexpr auto operator[](Integer index) noexcept -> reference {
    return data_[to_storage_size(index)];
  }

  template <std::integral Integer>
  [[nodiscard]] constexpr auto operator[](Integer index) const noexcept -> const_reference {
    return data_[to_storage_size(index)];
  }

  template <std::integral Integer>
  [[nodiscard]] constexpr auto at(Integer index) -> reference {
    return data_.at(to_storage_size(index));
  }

  template <std::integral Integer>
  [[nodiscard]] constexpr auto at(Integer index) const -> const_reference {
    return data_.at(to_storage_size(index));
  }

  [[nodiscard]] constexpr auto front() noexcept -> reference { return data_.front(); }
  [[nodiscard]] constexpr auto front() const noexcept -> const_reference { return data_.front(); }

  [[nodiscard]] constexpr auto back() noexcept -> reference { return data_.back(); }
  [[nodiscard]] constexpr auto back() const noexcept -> const_reference { return data_.back(); }

  [[nodiscard]] constexpr auto data() noexcept -> T*
    requires(!std::same_as<T, bool>)
  {
    return data_.data();
  }

  [[nodiscard]] constexpr auto data() const noexcept -> const T*
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

  constexpr auto insert(const_iterator pos, const T& value) -> iterator {
    return data_.insert(pos, value);
  }

  constexpr auto insert(const_iterator pos, T&& value) -> iterator {
    return data_.insert(pos, std::move(value));
  }

  template <std::integral Integer>
  constexpr auto insert(const_iterator pos, Integer count, const T& value) -> iterator {
    return data_.insert(pos, to_storage_size(count), value);
  }

  template <std::input_iterator InputIt>
  constexpr auto insert(const_iterator pos, InputIt first, InputIt last) -> iterator {
    return data_.insert(pos, first, last);
  }

  constexpr auto insert(const_iterator pos, std::initializer_list<T> values) -> iterator {
    return data_.insert(pos, values);
  }

  template <typename... Args>
  constexpr auto emplace(const_iterator pos, Args&&... args) -> iterator {
    return data_.emplace(pos, std::forward<Args>(args)...);
  }

  constexpr auto erase(const_iterator pos) -> iterator { return data_.erase(pos); }

  constexpr auto erase(const_iterator first, const_iterator last) -> iterator {
    return data_.erase(first, last);
  }

  constexpr auto push_back(const T& value) -> void { data_.push_back(value); }
  constexpr auto push_back(T&& value) -> void { data_.push_back(std::move(value)); }

  template <typename... Args>
  constexpr auto emplace_back(Args&&... args) -> reference {
    return data_.emplace_back(std::forward<Args>(args)...);
  }

  constexpr auto pop_back() -> void { data_.pop_back(); }

  template <std::integral Integer>
  constexpr auto resize(Integer count) -> void {
    data_.resize(to_storage_size(count));
  }

  template <std::integral Integer>
  constexpr auto resize(Integer count, const T& value) -> void {
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

  // Explicit access for APIs that specifically require std::vector.
  [[nodiscard]] constexpr auto std_vector() noexcept -> storage_type& { return data_; }
  [[nodiscard]] constexpr auto std_vector() const noexcept -> const storage_type& { return data_; }

  friend constexpr auto operator==(const Vector&, const Vector&) -> bool = default;

  friend constexpr auto operator<=>(const Vector& lhs, const Vector& rhs) -> decltype(auto)
    requires requires { lhs.data_ <=> rhs.data_; }
  {
    return lhs.data_ <=> rhs.data_;
  }
};

template <std::input_iterator InputIt,
          typename T = typename std::iterator_traits<InputIt>::value_type,
          typename Allocator = std::allocator<T>>
Vector(InputIt, InputIt, Allocator = Allocator()) -> Vector<T, Allocator>;

#if defined(__cpp_lib_containers_ranges) && __cpp_lib_containers_ranges >= 202202L
template <std::ranges::input_range R,
          typename Allocator = std::allocator<std::ranges::range_value_t<R>>>
Vector(std::from_range_t from_range, R&& values, Allocator alloc = Allocator())
    -> Vector<std::ranges::range_value_t<R>, Allocator>;
#endif

template <typename T, typename Allocator>
constexpr auto swap(Vector<T, Allocator>& lhs,
                    Vector<T, Allocator>& rhs) noexcept(noexcept(lhs.swap(rhs))) -> void {
  lhs.swap(rhs);
}

// ADL equivalents of std::erase / std::erase_if, returning int.
template <typename T, typename Allocator, typename U>
constexpr auto erase(Vector<T, Allocator>& values, const U& value) -> int {
  return static_cast<int>(std::erase(values.std_vector(), value));
}

template <typename T, typename Allocator, typename Predicate>
constexpr auto erase_if(Vector<T, Allocator>& values, Predicate pred) -> int {
  return static_cast<int>(std::erase_if(values.std_vector(), std::move(pred)));
}
