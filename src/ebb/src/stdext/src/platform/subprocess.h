#ifndef __PLATFORM_SUBPROCESS_H__
#define __PLATFORM_SUBPROCESS_H__

#include "stdext/file_system.h"
#include "stdext/optional.h"

namespace platform {

// Executes the given system command, with stdout/stderr/stdin redirected to
// or from the given files, or null if files are not provided.
int SystemCommand(
    const char* command,
    const stdext::optional<stdext::file_system::Path>& stdout_file,
    const stdext::optional<stdext::file_system::Path>& stderr_file,
    const stdext::optional<stdext::file_system::Path>& stdin_file);

}  // namespace platform

#endif  // __PLATFORM_SUBPROCESS_H__
