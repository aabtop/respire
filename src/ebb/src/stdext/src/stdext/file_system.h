#ifndef __STDEXT_FILE_SYSTEM_H__
#define __STDEXT_FILE_SYSTEM_H__

#include <cstring>
#include <iostream>
#include <string>

#include "platform/file_system.h"

namespace stdext {
namespace file_system {

class PathStrRef {
 public:
  PathStrRef(const char* str) : str_(str) {}
  PathStrRef(const std::string* str) : str_(str->c_str()) {}
  PathStrRef(const PathStrRef& other) : str_(other.str_) {}

  const char* c_str() const { return str_; }

 private:
  const char* str_;
};

class Path {
 public:
  Path(const char* str) : owned_path_(str), ref_(owned_path_.c_str()) {}
  Path(const std::string& str)
      : owned_path_(str), ref_(owned_path_.c_str()) {}
  Path(std::string&& str)
      : owned_path_(std::move(str)), ref_(owned_path_.c_str()) {}
  Path(const Path& other)
      : owned_path_(other.owned_path_), ref_(owned_path_.c_str()) {}
  Path(Path&& other)
      : owned_path_(std::move(other.owned_path_)),
        ref_(owned_path_.c_str()) {}
  Path(const PathStrRef& other)
      : owned_path_(other.c_str()), ref_(owned_path_.c_str()) {}

  operator PathStrRef() const { return ref_; }
  bool operator==(const Path& rhs) const {
    return owned_path_ == rhs.owned_path_;
  }

  const std::string& str() const { return owned_path_; }
  const char* c_str() const { return ref_.c_str(); }

  Path pop() const {
    auto last_index = owned_path_.rfind(platform::kPathSeparator);
    assert(1 == strlen(platform::kPathSeparator));
    if (last_index == std::string::npos ||
        last_index == owned_path_.length() - 1) {
      return *this;
    } else {
      return owned_path_.substr(0, last_index);
    }
  }

 private:
  const std::string owned_path_;
  PathStrRef ref_;
};

inline Path Join(PathStrRef a, PathStrRef b) {
  return Path(
      std::string(a.c_str()) +
      std::string(platform::kPathSeparator) +
      std::string(b.c_str()));
}

// Returns the time at which the file described in |filepath| was last modified.
// If the file does not exist, 
inline stdext::optional<std::chrono::system_clock::time_point>
    GetLastModificationTime(const PathStrRef& path) {
  return platform::GetLastModificationTime(path.c_str());
}

// When constructed will create a temporary directory and provide its path
// via path().  When destructed, the directory and everything in it will be
// deleted.
class TemporaryDirectory {
 public:
  TemporaryDirectory() {
    stdext::optional<std::string> str = platform::MakeTemporaryDirectory();
    if (str.has_value()) {
      path_.emplace(std::move(*str));
    }
  }

  ~TemporaryDirectory() {
    if (path_.has_value()) {
      if (!platform::RemoveDirectoryTree(path_->c_str())) {
        std::cerr << "Error removing temporary directory: " << path_->c_str()
                  << std::endl;
      }
    }
  }

  bool error() const { return !path_.has_value(); }
  const Path& path() const { return *path_; }

 private:
  optional<Path> path_;
};

inline Path GetThisModulePath() {
  return Path(platform::GetThisModulePath());
}

}  // namespace file_system
}  // namespace stdext

namespace std {
template <>
struct hash<stdext::file_system::Path>
{
  std::size_t operator()(const stdext::file_system::Path& key) const
  {
    hash<std::string> string_hasher;
    return string_hasher(key.str());
  }
};
}  // namespace std

#endif  // __STDEXT_FILE_SYSTEM_H__
