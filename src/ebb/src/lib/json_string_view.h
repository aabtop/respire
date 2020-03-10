#ifndef __EBB_LIB_JSON_STRING_VIEW_H__
#define __EBB_LIB_JSON_STRING_VIEW_H__

#include <cstring>

#include "stdext/string_view.h"
#include "stdext/file_system.h"
#include <string>

namespace ebb {
namespace lib {

// Returns an output string that is properly JSON-escaped.
std::string ToJSON(const std::string& source);

class JSONStringView {
 public:
  JSONStringView(const char* data) : string_view_(data, strlen(data)) {}
  JSONStringView(const std::string& data)
      : string_view_(data.data(), data.size()) {}
  JSONStringView(const char* data, size_t size) : string_view_(data, size) {}
  JSONStringView(JSONStringView&& rhs) = default;
  JSONStringView(const JSONStringView& rhs) = default;
  JSONStringView(stdext::string_view string_view) : string_view_(string_view) {}

  bool operator==(const JSONStringView& rhs) const {
    return string_view_ == rhs.string_view_;
  }

  stdext::string_view string_view() const { return string_view_; }

  std::string AsString() const;

  bool IsEqual(const char* str) const;

 private:
   stdext::string_view string_view_;
};

class JSONPathStringView : public JSONStringView {
 public:
  JSONPathStringView(JSONStringView string_view)
      : JSONStringView(string_view) {}

  stdext::file_system::Path AsPath() const {
    return stdext::file_system::Path(AsString());
  }
};

}  // namespace lib
}  // namespace ebb

namespace std {
template <>
struct hash<ebb::lib::JSONPathStringView>
{
  std::size_t operator()(const ebb::lib::JSONPathStringView& key) const
  {
    hash<stdext::string_view> string_hasher;
    return string_hasher(key.string_view());
  }
};
}  // namespace std

#endif  // __EBB_LIB_JSON_STRING_VIEW_H__