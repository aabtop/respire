#ifndef __STDEXT_VARIANT_H__
#define __STDEXT_VARIANT_H__

#include <cassert>

#include "stdext/align.h"
#include "stdext/type_id.h"
#include "stdext/types.h"

namespace stdext {

template <typename... Types>
class variant;

template <typename T, typename... Types>
bool holds_alternative(const variant<Types...>& v);

template <typename T, typename... Types>
const T& get(const variant<Types...>& v);

template <typename T, typename... Types>
T& get(variant<Types...>& v);

namespace internal {

template <typename Head>
bool DestructHelper(TypeId id, void* memory) {
  if (id == GetTypeId<Head>()) {
    reinterpret_cast<Head*>(memory)->~Head();
    return true;
  }
  return false;
};

template <typename Head1, typename Head2, typename... Tail>
void DestructHelper(TypeId id, void* memory) {
  if (!DestructHelper<Head1>(id, memory)) {
    DestructHelper<Head2, Tail...>(id, memory);
  }
}

template <typename V, typename Head>
bool EqualsHelper(const V& lhs, const V& rhs) {
  if (holds_alternative<Head>(lhs) != holds_alternative<Head>(rhs)) {
    return false;
  }
  return get<Head>(lhs) == get<Head>(rhs);
};

template <typename V, typename Head1, typename Head2, typename... Tail>
bool EqualsHelper(const V& lhs, const V& rhs) {
  if (holds_alternative<Head1>(lhs)) {
    return EqualsHelper<V, Head1>(lhs, rhs);
  } else {
    return EqualsHelper<V, Head2, Tail...>(lhs, rhs);
  }
}

template <typename V, typename Head>
bool EmplaceHelper(void* memory, const V& rhs) {
  if (holds_alternative<Head>(rhs)) {
    new (memory) Head(get<Head>(rhs));
    return true;
  } else {
    return false;
  }
};

template <typename V, typename Head1, typename Head2, typename... Tail>
bool EmplaceHelper(void* memory, const V& rhs) {
  if (EmplaceHelper<V, Head1>(memory, rhs)) {
    return true;
  } else {
    return EmplaceHelper<V, Head2, Tail...>(memory, rhs);
  }
}

template <typename V, typename Head>
bool EmplaceHelper(void* memory, V&& rhs) {
  if (holds_alternative<Head>(rhs)) {
    new (memory) Head(get<Head>(std::forward<V>(rhs)));
    return true;
  } else {
    return false;
  }
};

template <typename V, typename Head1, typename Head2, typename... Tail>
bool EmplaceHelper(void* memory, V&& rhs) {
  if (EmplaceHelper<V, Head1>(memory, std::forward<V>(rhs))) {
    return true;
  } else {
    return EmplaceHelper<V, Head2, Tail...>(memory, std::forward<V>(rhs));
  }
}

template<typename T, typename... Tail>
struct is_in {
  static constexpr bool value = false;
};

template<typename T, typename Head, typename... Tail>
struct is_in<T, Head, Tail...> {
  static constexpr bool value = ::std::is_same<T, Head>::value ||
                                is_in<T, Tail...>::value;
};

}  // namespace internal

template <typename... Types>
class variant {
 public:
  // TODO: Implement a constructors.
  variant() = delete;
  variant(variant& rhs) : type_id_(rhs.type_id_) {
    internal::EmplaceHelper<variant<Types...>, Types...>(memory_.get(), rhs);
  }
  variant(const variant& rhs) : type_id_(rhs.type_id_) {
    internal::EmplaceHelper<variant<Types...>, Types...>(memory_.get(), rhs);
  }
  variant(variant&& rhs) : type_id_(rhs.type_id_) {
    internal::EmplaceHelper<variant<Types...>, Types...>(
        memory_.get(), std::forward<variant<Types...>>(rhs));
  }

  template<typename T, typename... Args>
  variant(in_place_type_t<T>, Args&&... args)
      : type_id_(GetTypeId<T>()) {
    static_assert(
        internal::is_in<typename std::decay<T>::type, Types...>::value,
        "Invalid type assigned to a variant.");
    new (memory_.get()) T(std::forward<Args>(args)...);
  }

  template<typename T>
  variant(T&& t) : type_id_(GetTypeId<T>()) {
    static_assert(
        internal::is_in<typename std::decay<T>::type, Types...>::value,
        "Invalid type assigned to a variant.");
    new (memory_.get()) typename std::decay<T>::type(std::forward<T>(t));
  }

  ~variant() {
    internal::DestructHelper<Types...>(type_id_, memory_.get());
  }

 private:
  union_memory<Types...> memory_;
  TypeId type_id_;

  template <typename T, typename... TypesF>
  friend bool holds_alternative(const variant<TypesF...>& v);

  template <typename T, typename... TypesF>
  friend const T& get(const variant<TypesF...>& v);

  template <typename T, typename... TypesF>
  friend T& get(variant<TypesF...>& v);

  template <typename T, typename... TypesF>
  friend T&& get(variant<TypesF...>&& v);
};

template <typename T, typename... Types>
bool holds_alternative(const variant<Types...>& v) {
  static_assert(
      internal::is_in<typename std::decay<T>::type, Types...>::value,
      "Invalid type assigned to a variant.");
  return v.type_id_ == GetTypeId<T>();
}

template <typename T, typename... Types>
const T& get(const variant<Types...>& v) {
  static_assert(
      internal::is_in<typename std::decay<T>::type, Types...>::value,
      "Invalid type assigned to a variant.");
  assert(holds_alternative<T>(v));
  return *reinterpret_cast<const T*>(v.memory_.get());
}

template <typename T, typename... Types>
T& get(variant<Types...>& v) {
  static_assert(
      internal::is_in<typename std::decay<T>::type, Types...>::value,
      "Invalid type assigned to a variant.");
  assert(holds_alternative<T>(v));
  return *reinterpret_cast<T*>(v.memory_.get());
}

template <typename T, typename... Types>
T&& get(variant<Types...>&& v) {
  static_assert(
      internal::is_in<typename std::decay<T>::type, Types...>::value,
      "Invalid type assigned to a variant.");
  assert(holds_alternative<T>(v));
  return std::move(*reinterpret_cast<T*>(v.memory_.get()));
}

template <typename... Types>
bool operator==(const variant<Types...>& lhs, const variant<Types...>& rhs) {
  return internal::EqualsHelper<variant<Types...>, Types...>(lhs, rhs);
}

template <typename T, typename... Types>
bool operator==(const T& lhs, const variant<Types...>& rhs) {
  static_assert(
      internal::is_in<typename std::decay<T>::type, Types...>::value,
      "Invalid type assigned to a variant.");

  if (!holds_alternative<T>(rhs)) {
    return false;
  } else {
    return lhs == get<T>(rhs);
  }
}

template <typename T, typename... Types>
bool operator==(const variant<Types...>& lhs, const T& rhs) {
  return operator==(rhs, lhs);
}

}  // namespace stdext

#endif  // __STDEXT_VARIANT_H__
