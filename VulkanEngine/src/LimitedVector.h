#pragma once

#include <array>

template<typename T, size_t N>
class limited_vector {
 public:
  using value_type = T;
  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;

  using iterator = typename std::array<T, N>::iterator;
  using const_iterator = typename std::array<T, N>::const_iterator;
  using reverse_iterator = typename std::array<T, N>::reverse_iterator;
  using const_reverse_iterator =
      typename std::array<T, N>::const_reverse_iterator;

  limited_vector() : count_(0) {}
  ~limited_vector() {}

  limited_vector(const limited_vector<T, N>& other) { data_ = other; }
  limited_vector<T, N>& operator=(const limited_vector<T, N>& other) {
    data_ = other;
  }
  limited_vector(limited_vector<T, N>&& other) { data_ = std::move(other); }
  limited_vector<T, N>& operator=(limited_vector<T, N>&& other) {
    data_ = std::move(other);
  }

  constexpr reference at(size_t index) { return data_.at(index); }
  constexpr const_reference at(size_t index) const { return data_.at(index); }
  constexpr reference operator[](size_t index) noexcept {
    return data_[index];
  }
  constexpr const_reference operator[](size_t index) const noexcept {
    return data_[index];
  }

  constexpr reference back() noexcept { return data_[count_ - 1]; }
  constexpr reference front() noexcept { return data_[0]; }
  constexpr const_reference back() const noexcept { return data_[count_ - 1]; }
  constexpr const_reference front() const noexcept { return data_[0]; }

  constexpr const_pointer data() const noexcept { return data_.data(); }
  constexpr pointer data() noexcept { return data_.data(); }

  constexpr size_t size() const noexcept { return count_; }
  constexpr size_t max_size() const noexcept { return N; }
  constexpr bool empty() const noexcept { return count_ == 0; }

  template <typename... Args>
  void emplace_back(Args&&... args) {
    if (count_ < N) {
      data_[count_] = T(std::forward<Args>(args)...);
      ++count_;
    }
  }

  void push_back(const T& val) noexcept {
    if (count_ < N) {
      data_[count_] = val;
      ++count_;
    }
  }
  void push_back(T&& val) noexcept {
    if (count_ < N) {
      data_[count_] = std::move(val);
      ++count_;
    }
  }
  void pop_back() noexcept {
    if (count_ > 0) --count_;
  }

  void clear() noexcept { count_ = 0; }

  constexpr iterator begin() noexcept { return data_.begin(); }
  constexpr iterator end() noexcept { return data_.end(); }
  constexpr const_iterator cbegin() const noexcept { return data_.cbegin(); }
  constexpr const_iterator cend() const noexcept { return data_.cend(); }
  constexpr reverse_iterator rbegin() noexcept { return data_.rbegin(); }
  constexpr reverse_iterator rend() noexcept { return data_.rend(); }
  constexpr const_reverse_iterator crbegin() const noexcept {
    return data_.crbegin();
  }
  constexpr const_reverse_iterator crend() const noexcept {
    return data_.crend();
  }

 private:
  size_t count_;
  std::array<T, N> data_;
};