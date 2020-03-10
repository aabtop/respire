#ifndef __STDEXT_FIXED_VECTOR_H__
#define __STDEXT_FIXED_VECTOR_H__

#include <cassert>

#include "stdext/align.h"

namespace stdext {

template <typename T>
class fixed_vector {
 public:
  fixed_vector(const fixed_vector& other) = delete;
  fixed_vector(fixed_vector&& other) = delete;

  fixed_vector(void* memory, size_t capacity)
      : memory_(memory), capacity_(capacity), size_(0) {}

  ~fixed_vector() {
    clear();
  }

  template <typename... U>
  void emplace_back(U&&... u) {
    assert(size_ < capacity_);

    new (data() + size_) T(::std::forward<U>(u)...);

    ++size_;
  }

  void resize(size_t size) {
    if (size < size_) {
      ReduceSizeTo(size);
    } else {
      for (size_t i = size_; i < size; ++i) {
        new (data() + i) T();
      }
      size_ = size;
    }
  }

  bool empty() const { return size_ == 0; }

  size_t size() const { return size_; }

  T& operator[](size_t index) {
    assert(index < size_);
    return data()[index];
  }

  const T& operator[](size_t index) const {
    assert(index < size_);
    return data()[index];
  }

  T* data() { return reinterpret_cast<T*>(memory_); }

  const T* data() const { return reinterpret_cast<const T*>(memory_); }

  size_t capacity() const { return capacity_; }

  void clear() {
    ReduceSizeTo(0);
  }

 protected:
  fixed_vector() : memory_(nullptr), size_(0), capacity_(0) {}

  void Initialize(void* memory, size_t capacity) {
    memory_ = memory;
    capacity_ = capacity;
    size_ = 0;
  }

 private:
  void ReduceSizeTo(size_t reduce_to) {
    assert(reduce_to <= size_);
    for (size_t i = reduce_to; i < size_; ++i) {
      data()[i].~T();
    }
    size_ = reduce_to;
  }

  void* memory_;
  size_t size_;
  size_t capacity_;
};

template <typename T, size_t CAPACITY>
class fixed_vector_with_memory : public fixed_vector<T> {
 public:
  fixed_vector_with_memory() {
    fixed_vector<T>::Initialize(memory_.get(), CAPACITY);
  }

 private:
  aligned_memory<T, CAPACITY> memory_;
};

}  // namespace stdext

#endif  // __STDEXT_FIXED_VECTOR_H__