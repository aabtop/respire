#include "system_command_node.h"

#include <sstream>

#include "platform/subprocess.h"

namespace respire {

namespace {
stdext::optional<Error> ExecuteSystemCommand(
    Environment* env, const ebb::lib::JSONStringView* command,
    const stdext::optional<ebb::lib::JSONPathStringView>* stdout_file,
    const stdext::optional<ebb::lib::JSONPathStringView>* stderr_file,
    const stdext::optional<ebb::lib::JSONPathStringView>* stdin_file) {
  int error_code = env->system_command_function()(
      command->AsString().c_str(),
      *stdout_file ? (*stdout_file)->AsPath() :
          stdext::optional<stdext::file_system::Path>(),
      *stderr_file ? (*stderr_file)->AsPath() :
          stdext::optional<stdext::file_system::Path>(),
      *stdin_file ? (*stdin_file)->AsPath() :
          stdext::optional<stdext::file_system::Path>());
  if (error_code != 0) {
    std::ostringstream error_message;
    error_message << "Exit code " << error_code << "." << std::endl;
    return Error(error_message.str());
  } else {
    return stdext::nullopt;
  }
}
}  // namespace

SystemCommandNode::SystemCommandNode(
    Environment* env, std::vector<FileInfoNodeOutput>&& inputs,
    SystemCommandNodeParams&& params,
    FileProcessNode::GetDepsFunction get_deps_function)
    : activity_log_entry_(env->activity_log(), std::move(params)),
      file_process_node_(
          env, std::move(inputs), &activity_log_entry_.params().outputs,
          &activity_log_entry_.params().soft_outputs,
          std::bind(&ExecuteSystemCommand, env,
              &activity_log_entry_.params().command,
              &activity_log_entry_.params().stdout_file,
              &activity_log_entry_.params().stderr_file,
              &activity_log_entry_.params().stdin_file),
          get_deps_function, &activity_log_entry_) {}

}  // respire
