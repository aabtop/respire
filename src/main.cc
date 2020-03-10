#include "registry_node.h"

#include <iostream>
#include <vector>

#include "build_targets.h"
#include "environment.h"
#include "error.h"
#include "stdext/file_system.h"

namespace {

void PrintUsage() {
  std::cerr << "Usage: " << std::endl
            << "  respire [-j N] INITIAL_REGISTRY_FILE" << std::endl;
}

std::string WithDedupedBackslashes(const std::string input) {
  std::vector<char> output;
  output.reserve(input.size());

  for (size_t i = 0; i < input.size(); ++i) {
    output.push_back(input[i]);
    if (input[i] == '\\' && (i + 1) < input.size() && input[i + 1] == '\\') {
      ++i;
    }
  }

  return std::string(output.data(), output.size());
}

struct CommandLineParams {
  CommandLineParams(const stdext::file_system::Path& initial_file_path)
      : activity_log_level(respire::ActivityLog::Level::None),
        initial_file_path(initial_file_path) {}

  stdext::optional<int> num_threads;
  respire::ActivityLog::Level activity_log_level;

  stdext::file_system::Path initial_file_path;
};

stdext::optional<CommandLineParams> ParseArgs(
    int argc, const char** args) {
  if (argc < 2) {
    return stdext::nullopt;
  }

  CommandLineParams params{stdext::file_system::Path(
      WithDedupedBackslashes(args[argc - 1]))};

  for (int i = 1; i < argc - 1; ++i) {
    if (std::string(args[i]) == "-j") {
      if (argc < i + 3) {
        return stdext::nullopt;
      }
      params.num_threads = atoi(args[2]);
      ++i;
    } else if (std::string(args[i]) == "-o") {
      params.activity_log_level =
          respire::ActivityLog::Level::ProcessExecutionOnly;
    } else if (std::string(args[i]) == "-oo") {
      params.activity_log_level = respire::ActivityLog::Level::All;
    }
  }

  return params;
}

}  // namespace

int main(int argc, const char** args) {
  stdext::optional<CommandLineParams> command_line_params =
      ParseArgs(argc, args);
  if (!command_line_params) {
    PrintUsage();
    return 1;
  }

  // First parse out the registry from the registry files and build up our
  // graph.
  respire::Environment::Options options;
  options.num_threads = command_line_params->num_threads ?
      *command_line_params->num_threads : 1;
  options.activity_log_level = command_line_params->activity_log_level;

  respire::Environment env(options);

  respire::OptionalError maybe_error(respire::BuildTargets(
      &env, command_line_params->initial_file_path));

  if (maybe_error) {
    env.activity_log()->SignalRespireError(*maybe_error);
    return 1;
  }

  return 0;
}
