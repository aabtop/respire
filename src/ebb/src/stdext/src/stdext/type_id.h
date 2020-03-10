#ifndef __STDEXT_TYPE_ID_H__
#define __STDEXT_TYPE_ID_H__

#include <stdint.h>
#include <type_traits>

namespace stdext {

namespace internal {
template <typename T>
class TypeIdHelper {
 public:
  static bool dummy_;
};

template <typename T>
bool TypeIdHelper<T>::dummy_ = false;
}  // namespace internal


typedef uintptr_t TypeId;

template <typename T>
TypeId GetTypeId() {
  return reinterpret_cast<TypeId>(
      &(internal::TypeIdHelper<typename std::decay<T>::type>::dummy_));
}

}  // namespace stdext

#endif  // __STDEXT_TYPE_ID_H__
