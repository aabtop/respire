#ifndef __RESPIRE_ENVIRONMENT_H__
#define __RESPIRE_ENVIRONMENT_H__

#include <ebbpp.h>

#include "activity_log.h"
#include "platform/subprocess.h"
#include "stdext/file_system.h"
#include "stdext/optional.h"

namespace respire {

class RegistryNode;

class Environment {
 public:
  using SystemCommandFunction = std::function<int(
      const char*,
      const stdext::optional<stdext::file_system::Path>& stdout_file,
      const stdext::optional<stdext::file_system::Path>& stderr_file,
      const stdext::optional<stdext::file_system::Path>& stdin_file)>;

  struct Options {
    Options()
      : num_threads(1),
        system_command_function(&platform::SystemCommand),
        activity_log_level(ActivityLog::Level::None) {}

    int num_threads;
    SystemCommandFunction system_command_function;
    ActivityLog::Level activity_log_level;
  };

  Environment(const Options& options = Options());
  ~Environment();

  ebb::Environment* ebb_env() { return &ebb_env_; }

  const SystemCommandFunction& system_command_function() const {
    return system_command_function_;
  }

  ActivityLog* activity_log() { return &activity_log_; }

 private:
  ebb::Environment ebb_env_;

  SystemCommandFunction system_command_function_;

  ActivityLog activity_log_;
};

}  // namespace respire

#endif  // __RESPIRE_ENVIRONMENT_H__
