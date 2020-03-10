#include "platform/file_info.h"

#include <sys/types.h>
#include <sys/stat.h>

namespace platform {

stdext::optional<std::chrono::system_clock::time_point>
    GetLastModificationTime(const char* filepath) {
  struct stat buffer;

  if (stat(filepath, &buffer) != 0) {
    return stdext::optional<std::chrono::system_clock::time_point>();
  }

  return std::chrono::system_clock::from_time_t(buffer.st_mtime);
}

}  // namespace platform
