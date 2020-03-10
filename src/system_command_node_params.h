#ifndef __RESPIRE_SYSTEM_COMMAND_NODE_PARAMS_H__
#define __RESPIRE_SYSTEM_COMMAND_NODE_PARAMS_H__

#include <vector>

#include "lib/json_string_view.h"
#include "stdext/optional.h"

namespace respire {

// This is defined in a separate header file from SystemCommandNode in order
// to get around a cyclic dependency problem.

struct SystemCommandNodeParams {
  SystemCommandNodeParams(SystemCommandNodeParams&& rhs) = default;
  SystemCommandNodeParams(const SystemCommandNodeParams& rhs) = default;
  SystemCommandNodeParams(
      ebb::lib::JSONStringView command,
      std::vector<ebb::lib::JSONPathStringView>&& inputs,
      std::vector<ebb::lib::JSONPathStringView>&& outputs,
      std::vector<ebb::lib::JSONPathStringView>&& soft_outputs =
          std::vector<ebb::lib::JSONPathStringView>(),
      stdext::optional<ebb::lib::JSONPathStringView> deps_file
          = stdext::nullopt,
      stdext::optional<ebb::lib::JSONPathStringView> stdout_file
          = stdext::nullopt,
      stdext::optional<ebb::lib::JSONPathStringView> stderr_file
          = stdext::nullopt,
      stdext::optional<ebb::lib::JSONPathStringView> stdin_file
          = stdext::nullopt)
      : command(command), inputs(std::move(inputs)),
        outputs(std::move(outputs)), soft_outputs(std::move(soft_outputs)),
        deps_file(deps_file),
        stdout_file(stdout_file),
        stderr_file(stderr_file),
        stdin_file(stdin_file)  {}
  bool operator==(const SystemCommandNodeParams& rhs) const {
    return command == rhs.command &&
           inputs == rhs.inputs &&
           outputs == rhs.outputs &&
           soft_outputs == rhs.soft_outputs &&
           deps_file == rhs.deps_file &&
           stdout_file == rhs.stdout_file &&
           stderr_file == rhs.stderr_file &&
           stdin_file == rhs.stdin_file;
  }

  ebb::lib::JSONStringView command;
  std::vector<ebb::lib::JSONPathStringView> inputs;
  std::vector<ebb::lib::JSONPathStringView> outputs;
  std::vector<ebb::lib::JSONPathStringView> soft_outputs;
  stdext::optional<ebb::lib::JSONPathStringView> deps_file;
  stdext::optional<ebb::lib::JSONPathStringView> stdout_file;
  stdext::optional<ebb::lib::JSONPathStringView> stderr_file;
  stdext::optional<ebb::lib::JSONPathStringView> stdin_file;
};

}  // namespace respire

#endif  // __RESPIRE_SYSTEM_COMMAND_NODE_PARAMS_H__
