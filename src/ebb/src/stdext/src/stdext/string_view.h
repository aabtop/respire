#ifndef __STDEXT_STRING_VIEW_H__
#define __STDEXT_STRING_VIEW_H__

#include <functional>

#include "stdext/span.h"

#include "stdext/murmurhash/MurmurHash3.h"

namespace stdext {

using string_view = span<const char>;

}  // namespace stdext

namespace std {
template <>
struct hash<stdext::string_view>
{
  std::size_t operator()(const stdext::string_view& key) const
  {
    // We do unfortunately leave half of the hash zeroed out by this
    // implementation since we call a 32-bit hashing function on a (possibly)
    // 64 bit value.  Oh well!
    std::size_t result = 0;
    MurmurHash3_x86_32(key.data(), key.size(), 0, &result);
    return result;
  }
};
}  // namespace std

#endif  // __STDEXT_STRING_VIEW_H__