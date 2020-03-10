#ifndef __STDEXT_SPAN_H__
#define __STDEXT_SPAN_H__

#include <cstddef>

namespace stdext {

template <typename T>
class span {
 public:
  span(T* start, size_t size) : start_(start), size_(size) {}
  span(const span& rhs) : start_(rhs.start_), size_(rhs.size_) {}

  T* data() { return start_; }
  const T* data() const { return start_; }

  T* begin() { return start_; }
  T* end() { return start_ + size_; }
  const T* begin() const { return start_; }
  const T* end() const { return start_ + size_; }

  size_t size() const { return size_; }

  T& operator[](size_t index) { return start_[index]; }
  const T& operator[](size_t index) const { return start_[index]; }

  bool operator==(const span& rhs) const {
    if (size_ != rhs.size_) {
      return false;
    }
    for (size_t i = 0; i < size_; ++i) {
      if (!(start_[i] == rhs.start_[i])) {
        return false;
      }
    }
    return true;
  }

 private:
  T* start_;
  size_t size_;
};

template <typename T>
span<T> make_span(T* start, size_t size) {
  return span<T>(start, size);
}

}  // namespace stdext

#endif  // __STDEXT_SPAN_H__
