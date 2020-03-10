#include "platform/file_system.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <ftw.h>
#include <unistd.h>

#include <iostream>

namespace platform {

stdext::optional<std::chrono::system_clock::time_point>
    GetLastModificationTime(const char* filepath) {
  struct stat buffer;

  if (stat(filepath, &buffer) != 0) {
    return stdext::optional<std::chrono::system_clock::time_point>();
  }

  return std::chrono::system_clock::from_time_t(buffer.st_mtim.tv_sec)
      + std::chrono::nanoseconds(buffer.st_mtim.tv_nsec);
}

const char* kPathSeparator = "/";

const bool kFileModificationTimeConsistentWithSystemClock = false;

stdext::optional<std::string> MakeTemporaryDirectory() {
  std::string temp_directory("/tmp/stdext_platform_XXXXXX");
  if (mkdtemp(const_cast<char*>(temp_directory.c_str())) == nullptr) {
    return stdext::optional<std::string>();
  }

  return temp_directory;
}

namespace {
int RemoveFile(
    const char *pathname, const struct stat *sbuf, int type, struct FTW *ftwb) {
  if (remove(pathname) < 0) {
    std::cerr << "Error removing path: " << pathname << std::endl;
    return -1;
  }

  return 0;
}
}  // namespace

bool RemoveDirectoryTree(const char* filepath) {
  // At least one file descriptor will be used for each directory level, so
  // this value effectively limits the directory depth we can traverse to.
  int kFDLimit = 20;

  if (nftw(filepath, RemoveFile, kFDLimit,
           FTW_DEPTH | FTW_MOUNT | FTW_PHYS) < 0) {
    std::cerr << "Error removing directory tree: " << filepath << std::endl;
    return false;
  }

  return true;
}

std::string GetThisModulePath() {
  const size_t kBufferSize = 512;
  char path_buffer[kBufferSize];
  ssize_t bytes_read = readlink("/proc/self/exe", path_buffer, kBufferSize);
  assert(bytes_read < kBufferSize);
  path_buffer[bytes_read] = '\0';
  return std::string(path_buffer);
}

}  // namespace platform
