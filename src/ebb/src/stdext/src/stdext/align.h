#ifndef __STDEXT_ALIGN_H__
#define __STDEXT_ALIGN_H__

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "stdext/numeric.h"

namespace stdext {

constexpr uintptr_t align_up(uintptr_t value, int32_t alignment) {
  return (value + (alignment - 1)) & ~(alignment - 1);
}

constexpr bool aligned(uintptr_t value, int32_t alignment) {
  return value == align_up(value, alignment);
}

namespace internal {

template <size_t SIZE, size_t ALIGNMENT, size_t NUM_ITEMS = 1>
class aligned_memory
{
public:
  void* get() { return mem_; }
  const void* get() const { return mem_; }

private:
  alignas(ALIGNMENT) int8_t mem_[SIZE * NUM_ITEMS];
};

template <typename T>
constexpr T max(T a, T b) {
  return a > b ? a : b;
}

template<typename T>
constexpr size_t max_size() { return sizeof(T); }

template<typename T1, typename T2, typename... U>
constexpr size_t max_size() {
  return max(sizeof(T1), max_size<T2, U...>());
}

template<typename T>
constexpr size_t alignment_lcm() { return alignof(T); }

template<typename T1, typename T2, typename... U>
constexpr size_t alignment_lcm() {
  return lcm(alignof(T1), alignment_lcm<T2, U...>());
}

}  // namespace internal

template<typename T, size_t NUM_ITEMS = 1>
class aligned_memory :
    public internal::aligned_memory<sizeof(T), alignof(T), NUM_ITEMS> {};

template<typename... Types>
class union_memory :
    public internal::aligned_memory<internal::max_size<Types...>(),
                                    internal::alignment_lcm<Types...>(), 1> {};

}  // namespace stdext

#endif  // ifndef __STDEXT_ALIGN_H__