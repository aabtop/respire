#ifndef __RESPIRE_TEST_SYSTEM_COMMAND_H__
#define __RESPIRE_TEST_SYSTEM_COMMAND_H__

#include "stdext/file_system.h"
#include "stdext/optional.h"

namespace respire {

// We define a simple shell here that we can ensure is cross-platform (via
// the STL fstream functions) in order to test that we can properly execute some
// simple commands to produce files.
int TestSystemCommand(
    const char* command,
    const stdext::optional<stdext::file_system::Path>& stdout_file,
    const stdext::optional<stdext::file_system::Path>& stderr_file,
    const stdext::optional<stdext::file_system::Path>& stdin_file);

}  // namespace respire

#endif  // __RESPIRE_TEST_SYSTEM_COMMAND_H__
