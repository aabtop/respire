#ifndef __RESPIRE_ACTIVITY_LOG_H__
#define __RESPIRE_ACTIVITY_LOG_H__

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "file_info_node.h"
#include "registry_parser.h"
#include "lib/json_string_view.h"
#include "stdext/file_system.h"
#include "stdext/optional.h"
#include "system_command_node_params.h"

namespace respire {

class ActivityLog {
 public:
  enum class Level {
    // No logging
    None,
    // Only log process execution events, ignore less important events such
    // as starting dependency scanning.
    ProcessExecutionOnly,
    // Log all events.
    All,
  };

  ActivityLog(std::ostream* ostream, Level level)
      : level_(level), ostream_(ostream) {}

  class FileProcessNodeLog {
   public:
    FileProcessNodeLog(
        ActivityLog* activity_log, SystemCommandNodeParams&& params);
    ~FileProcessNodeLog();

    const SystemCommandNodeParams& params() const {
        return params_;
    }

    // To be called when scanning of dependencies begins.
    void SignalStartDependencyScan(bool dry_run);

    // To be called when command execution is about to begin.
    void SignalStartRunningCommand(bool dry_run);

    // To be called when processing is complete.  We may jump here straight
    // from dependency scanning, if it was found that all dependencies were
    // up-to-date.
    void SignalProcessingComplete(const stdext::optional<Error>& error,
                                  bool dry_run);

   private:
    void EnsureCreateSystemCommandNodeSignaled();

    ActivityLog* activity_log_;
    SystemCommandNodeParams params_;
    int node_id_ = -1;
    bool running_command_ = false;
  };

  class RegistryNodeLog {
   public:
    RegistryNodeLog(ActivityLog* activity_log,
                    const ebb::lib::JSONPathStringView& path);
    ~RegistryNodeLog();

    // To be called when scanning of dependencies begins.
    void SignalStartDependencyScan();

    // Indicates that parsing and processing of the registry file has begun.
    void SignalStartParsingRegistryFile();

    // Indicate that all parsing and processing of the registry file has
    // completed.
    void SignalProcessingComplete(const stdext::optional<Error>& error);

   private:
    void EnsureCreateRegistryNodeSignaled();

    ActivityLog* activity_log_;
    int node_id_ = -1;
    ebb::lib::JSONPathStringView path_;
  };

  // Called to log errors in the core respire execution flow.
  void SignalRespireError(const Error& error);

 private:
    friend class FileProcessNodeLog;
    friend class RegistryNodeLog;

    bool IsEnabled() const { return level_ != Level::None && ostream_; }
    int CreateNodeId();
    void Log(const std::string& str);
    void OutputNodeHeader(
        std::ostream* oss, int node_id, const char* event_type);

    const Level level_;

    std::ostream* const ostream_;

    std::mutex mutex_;

    int next_node_id_ = 0;

    std::chrono::time_point<std::chrono::steady_clock> start_time_ =
        std::chrono::steady_clock::now();
};

}  // namespace respire

#endif  // __RESPIRE_ACTIVITY_LOG_H__
