#include "lib/json_string_view.h"

#include <vector>
#include <sstream>
#include <string>

namespace ebb {
namespace lib {

std::string ToJSON(const std::string& source) {
  std::vector<char> output;
  output.reserve(source.size());

  for (char c : source) {
    switch (c) {
      case '"':
      case '\\': {
        output.push_back('\\');
        output.push_back(c);
      } break;
      default: {
        output.push_back(c);
      }
    };
  }

  return std::string(output.data(), output.size());
}

std::string JSONStringView::AsString() const {
  std::vector<char> output;
  output.reserve(string_view_.size());

  bool escaping = false;
  for (char c : string_view_) {
    if (escaping) {
      output.push_back(c);
      escaping = false;
    } else {
      if (c == '\\') {
        escaping = true;
      } else {
        output.push_back(c);
      }
    }
  }

  return std::string(output.data(), output.size());
}

bool JSONStringView::IsEqual(const char* str) const {
  for (char c : string_view_) {
    if (*str == '\0') {
      return false;
    }
    if (c != *str) {
      return false;
    }
    ++str;
  }
  return *str == '\0';
}

}  // namespace lib
}  // namespace ebb
