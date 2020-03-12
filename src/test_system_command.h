#ifndef __RESPIRE_TEST_SYSTEM_COMMAND_H__
#define __RESPIRE_TEST_SYSTEM_COMMAND_H__

#include "stdext/file_system.h"
#include "stdext/optional.h"

namespace respire {

// Helper function that waits a little bit in order to ensure that file writes
// that happen sequentially are recorded as being modified at different times.
// If file writes happen too quickly, then on some file systems where the
// modification time is coarse, files will appear to have been modified at
// the same time.  A value was chosen that seems to work well on most test
// platforms without slowing down the tests too much.  This is more important
// for tests where we want to make sure things get rebuilt properly, and in
// production if this problem occurs it just means we build more than we needed
// to.
void WaitForFilesystemTimeResolution();

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
