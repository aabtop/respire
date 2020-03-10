#ifndef __STDEXT_NUMERIC_H__
#define __STDEXT_NUMERIC_H__

namespace stdext {

template <typename T>
constexpr T gcd(T a, T b) {
  return (a == b ? a : (a > b ? gcd(a - b, b) : gcd(a, b - a)));
}

template <typename T>
constexpr T lcm(T a, T b) {
  return (a * b) / gcd(a, b);
}

}  // namespace stdext

#endif  // ifndef __STDEXT_NUMERIC_H__