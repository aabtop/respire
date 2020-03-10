#include "environment.h"
#include "registry_node.h"

namespace respire {

Environment::Environment(const Options& options)
    : ebb_env_(options.num_threads, 16 * 1024,
               // Setup the Ebb environment with a LIFO scheduling policy to
               // reduce the average amount of started but not completed
               // build tasks.
               ebb::Environment::SchedulingPolicy::LIFO),
      system_command_function_(options.system_command_function),
      activity_log_(
          options.activity_log_level != ActivityLog::Level::None ?
              &std::cout : nullptr,
          options.activity_log_level) {}

Environment::~Environment() {}

}  // namespace respire