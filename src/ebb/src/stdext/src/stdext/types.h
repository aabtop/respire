#ifndef __STDEXT_TYPES_H__
#define __STDEXT_TYPES_H__

namespace stdext {

struct in_place_t {
  explicit in_place_t() = default;
};
constexpr in_place_t in_place{};

template <typename T>
struct in_place_type_t {
  explicit in_place_type_t() = default;
};

}  // namespace stdext

#endif  // __STDEXT_TYPES_H__
