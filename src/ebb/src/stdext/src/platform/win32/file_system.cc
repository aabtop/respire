#include "platform/file_system.h"

#include <windows.h>

#include <ctime>
#include <iostream>
#include <memory>
#include <sstream>

namespace platform {

stdext::optional<std::chrono::system_clock::time_point>
    GetLastModificationTime(const char* filepath) {
  WIN32_FILE_ATTRIBUTE_DATA file_info = {0};
  if (GetFileAttributesEx(filepath, GetFileExInfoStandard, &file_info) == 0) {
    // If the file does not exist, return nothing.
    return stdext::optional<std::chrono::system_clock::time_point>();
  }

  uint64_t nanoseconds100 =
      (static_cast<uint64_t>(file_info.ftLastWriteTime.dwHighDateTime) << 32) +
      file_info.ftLastWriteTime.dwLowDateTime;

  return std::chrono::system_clock::from_time_t(0) +
    std::chrono::duration_cast<std::chrono::system_clock::duration>(
      std::chrono::nanoseconds(nanoseconds100 * 100) -
      std::chrono::seconds(11644473600LL));
}

const char* kPathSeparator = "\\";
const bool kFileModificationTimeConsistentWithSystemClock = false;

namespace {
std::string CreateRandomPath(const std::string& base_path) {
  std::ostringstream oss;
  oss <<  base_path;
  const int kNumRandomCharacters = 10;
  srand(static_cast<unsigned int>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count()));
  for (int i = 0; i < kNumRandomCharacters; ++i) {
    oss << static_cast<char>('a' + (rand() % 26));
  }
  return oss.str();
}
}  // namespace

stdext::optional<std::string> MakeTemporaryDirectory() {
  const int kMaxPathLength = 256;
  char base_path_buffer[kMaxPathLength];
  DWORD length = GetTempPath(kMaxPathLength, base_path_buffer);
  assert(length > 0);

  std::string base_path(base_path_buffer);

  const int kMaxRetries = 200;
  std::string random_path;
  for (int i = 0; i < kMaxRetries; ++i) {
    random_path = CreateRandomPath(base_path);
    if (!GetLastModificationTime(random_path.c_str())) {
      break;
    }
  }

  if (GetLastModificationTime(random_path.c_str())) {
    std::cerr << "Could not find unique temporary directory." << std::endl;
    return stdext::optional<std::string>();
  }

  // Create the new directory and return the path to it.
  if (!CreateDirectory(random_path.c_str(), NULL)) {
    std::cerr << "Error attempting to create temporary directory." << std::endl;
    return stdext::optional<std::string>();
  }

  return random_path;
}

bool RemoveDirectoryTree(const char* filepath) {
  // Turns out that the string we need to pass in here has to be *double*
  // null terminated.  So, set that up.
  std::string str(filepath);
  std::unique_ptr<char[]> double_null_str(new char[str.size() + 2]);
  memcpy(double_null_str.get(), str.c_str(), str.size());
  double_null_str[str.size()] = '\0';
  double_null_str[str.size() + 1] = '\0';

  SHFILEOPSTRUCT file_op = {
      NULL,
      FO_DELETE,
      double_null_str.get(),
      "",
      FOF_NOCONFIRMATION |
      FOF_NOERRORUI |
      FOF_SILENT,
      false,
      0,
      "" };
  int result = SHFileOperation(&file_op);
  return result == 0;
}

std::string GetThisModulePath() {
  HMODULE module = GetModuleHandleW(NULL);
  char path_str[MAX_PATH];
  GetModuleFileNameA(module, path_str, MAX_PATH);
  return std::string(path_str);
}

}  // namespace platform
