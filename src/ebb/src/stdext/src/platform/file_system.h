#ifndef __PLATFORM_FILE_SYSTEM_H__
#define __PLATFORM_FILE_SYSTEM_H__

#include "stdext/optional.h"
#include <cassert>
#include <chrono>
#include <functional>

namespace platform {

// Returns the time at which the file described in |filepath| was last modified.
// If the file does not exist, 
stdext::optional<::std::chrono::system_clock::time_point>
    GetLastModificationTime(const char* filepath);

stdext::optional<::std::string> MakeTemporaryDirectory();

bool RemoveDirectoryTree(const char* filepath);

// Returns the path of the executable file that spawned this process.
std::string GetThisModulePath();

extern const char* kPathSeparator;

extern const bool kFileModificationTimeConsistentWithSystemClock;

}  // namespace platform

#endif  // __PLATFORM_FILE_SYSTEM_H__
