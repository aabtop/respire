#ifndef __STDEXT_OPTIONAL_H__
#define __STDEXT_OPTIONAL_H__

#include <cassert>
#include <utility>

#include "stdext/align.h"

namespace stdext {

template <typename T>
class optional;

struct inplace_t {};
const inplace_t inplace{};

struct nullopt_t {};
const nullopt_t nullopt{};

// An optional represents an object that may or may not exist.  Space is
// reserved for the object upon construction, even if it does not exist
// immediately.  This is based off of the C++17 optional.
template <typename T>
class optional {
 public:
  optional() : has_value_(false) {
    SetObject();
  }
  optional(nullopt_t) : optional() {}
  optional(const T& rhs) : has_value_(true) {
    new (memory_.get()) T(rhs);
    SetObject();
  }
  optional(T&& rhs) : has_value_(true) {
    new (memory_.get()) T(std::move(rhs));
    SetObject();
  }
  optional(const optional& rhs) {
    if (rhs.has_value()) {
      new (memory_.get()) T(*rhs);
      has_value_ = true;
    } else {
      has_value_ = false;
    }
    SetObject();
  }
  optional(optional&& rhs) {
    if (rhs.has_value()) {
      new (memory_.get()) T(std::move(*rhs));
      has_value_ = true;
    } else {
      has_value_ = false;
    }
    SetObject();
  }

  template <typename... U>
  optional(const inplace_t&, U&&... u) : has_value_(true) {
    new (memory_.get()) T(std::forward<U>(u)...);
    SetObject();
  }

  ~optional();

  // Destructs the object and makes it not exist anymore.
  void reset();

  // Returns true if the object is constructed.
  bool has_value() const { return has_value_; }

  template <typename... U>
  void emplace(U&&... u) {
    reset();
    has_value_ = true;
    new (memory_.get()) T(std::forward<U>(u)...);
  }

  explicit operator bool() const { return has_value(); }

  T& value() { return *get(); }
  const T& value() const { return *get(); }

  T& operator*() { return *get(); }
  const T& operator*() const {  return *get(); }
  T* operator->() { return get(); }
  const T* operator->() const { return get(); }

  optional& operator=(const T& rhs) {
    emplace(rhs);
    return *this;
  }
  optional& operator=(T&& rhs) {
    emplace(std::move(rhs));
    return *this;
  }

  optional& operator=(const optional& rhs) {
    if (rhs) {
      emplace(*rhs);
    } else {
      reset();
    }
    return *this;
  }
  optional& operator=(optional&& rhs) {
    if (rhs) {
      emplace(std::move(*rhs));
    } else {
      reset();
    }
    return *this;
  }

  bool operator==(const optional& rhs) const {
    if (!has_value() && !rhs.has_value()) {
      return true;
    }
    if (!has_value() || !rhs.has_value()) {
      return false;
    }
    return value() == rhs.value();
  }

 private:
  T* get();
  const T* get() const;
  void SetObject();

  // True if the object has been constructed and not destructed.
  bool has_value_;

#if !defined(NDEBUG)
  // For debugging purposes to allow for easy viewing through a debugger.
  const T* object_;
#endif

  // The memory reserved for where the object will live.
  aligned_memory<T> memory_;
};

template <typename T>
optional<T>::~optional() {
  // Destroy the contained object before we return.
  reset();
}

template <typename T>
void optional<T>::reset() {
  // Destruct the object, if we own one.
  if (has_value_) {
    get()->~T();
    has_value_ = false;
  }
}

template <typename T>
T* optional<T>::get() {
  assert(has_value_);
  return reinterpret_cast<T*>(memory_.get());
}

template <typename T>
const T* optional<T>::get() const {
  assert(has_value_);
  return reinterpret_cast<const T*>(memory_.get());
}

template <typename T>
void optional<T>::SetObject() {
#if !defined(NDEBUG)
  object_ = reinterpret_cast<T*>(memory_.get());
#endif
}

}  // namespace stdext

#endif  // __STDEXT_OPTIONAL_H__