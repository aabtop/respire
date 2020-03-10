#include "platform/subprocess.h"

#include <fcntl.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

extern char** environ;

namespace platform {

namespace {
int AddRedirect(posix_spawn_file_actions_t* child_fd_actions, int fd,
                const stdext::optional<stdext::file_system::Path>& file_path,
                 int flags, mode_t file_mode) {
  if (file_path) {
    return posix_spawn_file_actions_addopen(
               child_fd_actions, fd, file_path->c_str(), flags, file_mode);
  } else {
    return posix_spawn_file_actions_addopen(
               child_fd_actions, fd, "/dev/null", O_RDONLY, 0);
  }
}
}

int SystemCommand(
    const char* command,
    const stdext::optional<stdext::file_system::Path>& stdout_file,
    const stdext::optional<stdext::file_system::Path>& stderr_file,
    const stdext::optional<stdext::file_system::Path>& stdin_file) {
  posix_spawn_file_actions_t child_fd_actions;
  if (posix_spawn_file_actions_init (&child_fd_actions) != 0) {
    return 1;
  }

  if (AddRedirect(
          &child_fd_actions, 1, stdout_file, O_WRONLY | O_CREAT | O_TRUNC,
          0644) != 0) {
    return 1;
  }

  if (AddRedirect(
          &child_fd_actions, 2, stderr_file, O_WRONLY | O_CREAT | O_TRUNC,
          0644) != 0) {
    return 1;
  }

  if (AddRedirect(
          &child_fd_actions, 0, stdin_file, O_RDONLY, 0) != 0) {
    return 1;
  }

  pid_t child_pid;
  const char* spawned_args[] = {"/bin/sh", "-c", command, NULL};

  if (posix_spawn(&child_pid, "/bin/sh", &child_fd_actions, NULL,
                  const_cast<char**>(spawned_args), environ) != 0) {
    return 1;
  }

  int status;
  if (waitpid(child_pid, &status, 0) == -1) {
    return 1;
  }

  return status;
}

}  // namespace platform
