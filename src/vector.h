#pragma once

#include <algorithm>
#include <cstddef>
#include <utility>

template <typename T>
class vector {
public:
  using value_type = T;

  using reference = T&;
  using const_reference = const T&;

  using pointer = T*;
  using const_pointer = const T*;

  using iterator = pointer;
  using const_iterator = const_pointer;

public:
  // O(1) nothrow
  vector() noexcept = default;

  // O(N) strong
  vector(const vector& other) : vector(other, other.size()) {
  }

  // O(1) strong
  vector(vector&& other)
      : data_{std::exchange(other.data_, nullptr)}
      , size_{std::exchange(other.size_, 0)}
      , capacity_{std::exchange(other.capacity_, 0)} {}

  // O(N) strong
  vector& operator=(const vector& other) {
    if (&other == this) {
      return *this;
    }
    vector(other).swap(*this);
    return *this;
  }

  // O(1) strong
  vector& operator=(vector&& other) {
    if (&other == this) {
      return *this;
    }
    vector{}.swap(*this);
    other.swap(*this);
    return *this;
  }

  // O(N) nothrow
  ~vector() noexcept {
    clear();
    operator delete(data_);
    data_ = nullptr;
  }

  // O(1) nothrow
  reference operator[](size_t index) {
    return data_[index];
  }

  // O(1) nothrow
  const_reference operator[](size_t index) const {
    return data_[index];
  }

  // O(1) nothrow
  pointer data() noexcept {
    return data_;
  }

  // O(1) nothrow
  const_pointer data() const noexcept {
    return data_;
  }

  // O(1) nothrow
  size_t size() const noexcept {
    return size_;
  }

  // O(1) nothrow
  reference front() {
    return *begin();
  }

  // O(1) nothrow
  const_reference front() const {
    return *begin();
  }

  // O(1) nothrow
  reference back() {
    return *(end() - 1);
  }

  // O(1) nothrow
  const_reference back() const {
    return *(end() - 1);
  }

  // O(1)* strong
  void push_back(T value) {
    if (size() < capacity()) {
      new (data_ + size()) T(std::move(value));
      ++size_;
      return;
    }
    size_t new_capacity = (capacity() == 0) ? 1 : capacity() * 2 + 1;
    vector tmp(*this, new_capacity);
    tmp.push_back(std::move(value));
    swap(tmp);
  }

  // O(1) nothrow
  void pop_back() {
    back().~T();
    size_--;
  }

  // O(1) nothrow
  bool empty() const noexcept {
    return size() == 0;
  }

  // O(1) nothrow
  size_t capacity() const noexcept {
    return capacity_;
  }

  // O(N) strong
  void reserve(size_t new_capacity) {
    if (new_capacity > capacity()) {
      vector(*this, new_capacity).swap(*this);
    }
  }

  // O(N) strong
  void shrink_to_fit() {
    if (size() != capacity()) {
      vector(*this, size()).swap(*this);
    }
  }

  // O(N) nothrow
  void clear() noexcept {
    size_t sz = size();
    for (size_t j = 0; j < sz; j++) {
      pop_back();
    }
    size_ = 0;
  }

  // O(1) nothrow
  void swap(vector& other) noexcept {
    std::swap(data_, other.data_);
    std::swap(size_, other.size_);
    std::swap(capacity_, other.capacity_);
  }

  // O(1) nothrow
  iterator begin() noexcept {
    return data_;
  }

  // O(1) nothrow
  iterator end() noexcept {
    return data_ + size_;
  }

  // O(1) nothrow
  const_iterator begin() const noexcept {
    return data_;
  }

  // O(1) nothrow
  const_iterator end() const noexcept {
    return data_ + size_;
  }

  // O(N) strong
  iterator insert(const_iterator pos, const T& value) {
    size_t index = pos - begin();
    push_back(value);
    for (iterator ptr = end() - 1; ptr != data() + index; ptr--) {
      std::iter_swap(ptr, ptr - 1);
    }
    return data() + index;
  }

  // O(N) nothrow(swap)
  iterator erase(const_iterator pos) {
    return erase(pos, pos + 1);
  }

  // O(N) nothrow(swap)
  iterator erase(const_iterator first, const_iterator last) {
    size_t f_index = first - begin();
    size_t l_index = last - begin();
    size_t diff = last - first;
    for (size_t i = 0; i < size() - l_index; i++) {
      std::swap(data_[l_index + i], data_[f_index + i]);
    }
    for (size_t i = 0; i < diff; i++) {
      pop_back();
    }
    return begin() + f_index;
  }

private:
  T* data_{nullptr};
  size_t size_{0};
  size_t capacity_{0};

  vector(const vector& other, size_t new_capacity) {
    if (new_capacity == 0 || &other == this) {
      return;
    }
    data_ = static_cast<T*>(operator new(sizeof(T) * new_capacity));
    for (size_t i = 0; i < other.size(); ++i) {
      try {
        new (data_ + i) T(other[i]);
      } catch (...) {
        for (size_t j = i; j > 0; --j) {
          data_[j - 1].~T();
        }
        operator delete(data_);
        throw;
      }
    }
    capacity_ = new_capacity;
    size_ = other.size();
  }
};
