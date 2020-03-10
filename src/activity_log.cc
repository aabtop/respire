#include "activity_log.h"

#include <sstream>

namespace respire {

namespace {
void OutputKey(std::ostream* oss, const char* key) {
    *oss << "  \"" << key << "\": ";
}

void OutputString(
    std::ostream* oss, const char* key, ebb::lib::JSONStringView str) {
  OutputKey(oss, key);
  *oss << "\"";
  if (str.string_view().data()[0] == '\"') {
    int foo = 0;
    ++foo;
  }

  oss->write(str.string_view().data(), str.string_view().size());
  *oss << "\", ";
}

namespace {
std::string ToJSON(const std::string& input) {
  std::ostringstream oss;
  for (const auto& c : input) {
    // TODO: This is sorrowly incomplete.
    switch (c) {
      case '"': {
        oss << "\\\"";
      } break;
      case '\n': {
        oss << "\\n";
      } break;
      case '\r': {
        oss << "\\r";
      } break;
      case '\\': {
        oss << "\\\\";
      } break;
      default:
        oss << c;
    }
  }
  return oss.str();
}
}  // namespace

void OutputRawString(
    std::ostream* oss, const char* key, const char* raw_string) {
  std::string json_string = ToJSON(raw_string);
  OutputString(oss, key, ebb::lib::JSONStringView(json_string.c_str()));
}

void OutputPathList(
    std::ostream* oss, const char* key,
    const std::vector<ebb::lib::JSONPathStringView>& path_list) {
  OutputKey(oss, key);
  *oss << "[";
  for (size_t i = 0; i < path_list.size(); ++i) {
    const auto& path = path_list[i];
    *oss << "    \"";
    oss->write(path.string_view().data(), path.string_view().size());
    *oss << "\"";
    if (i + 1 < path_list.size()) {
      *oss << ", ";
    }
  }
  *oss << "  ], ";
}

void MaybeOutputPath(
    std::ostream* oss, const char* key, 
    const stdext::optional<ebb::lib::JSONPathStringView>& path) {
  if (path) {
    OutputString(oss, key, *path);
  }
}

void OutputNodeFooter(std::ostream* oss) {
  // Add a dummy entry so that we never have a trailing comma.
  *oss << "\"d\":\"0\"},\n";
}

}  // namespace

void ActivityLog::OutputNodeHeader(
    std::ostream* oss, int node_id, const char* event_type) {
  *oss << "{";
  *oss << "\"id\": \"" << node_id << "\", ";
  *oss << "\"type\": \"" << event_type << "\", ";
  *oss << "\"time_us\": \"" <<
      std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::steady_clock::now() - start_time_).count() << "\", ";
}

ActivityLog::FileProcessNodeLog::FileProcessNodeLog(
    ActivityLog* activity_log,
    SystemCommandNodeParams&& params) :
    activity_log_(activity_log), params_(std::move(params)) {
  if (activity_log_->level_ != Level::All) {
    return;
  }

  EnsureCreateSystemCommandNodeSignaled();
}

ActivityLog::FileProcessNodeLog::~FileProcessNodeLog() {}

void ActivityLog::FileProcessNodeLog::SignalStartDependencyScan(bool dry_run) {
  if (activity_log_->level_ != Level::All) {
    return;
  }

  std::ostringstream oss;
  activity_log_->OutputNodeHeader(&oss, node_id_, "ScanningDependencies");
  if (dry_run) {
    OutputString(&oss, "dry_run", "true");
  }
  OutputNodeFooter(&oss);

  activity_log_->Log(oss.str());
}

void ActivityLog::FileProcessNodeLog::SignalStartRunningCommand(bool dry_run) {
  if (!activity_log_->IsEnabled()) {
    return;
  }

  EnsureCreateSystemCommandNodeSignaled();

  running_command_ = true;

  std::ostringstream oss;
  activity_log_->OutputNodeHeader(&oss, node_id_, "ExecutingCommand");
  if (dry_run) {
    // By flagging whether this is a dry run or not, and then always performing
    // a dry run before doing a real run, we enable the log output viewer to
    // learn how many nodes will need to be built.
    OutputString(&oss, "dry_run", "true");
  }
  OutputNodeFooter(&oss);

  activity_log_->Log(oss.str());
}

void ActivityLog::FileProcessNodeLog::SignalProcessingComplete(
    const stdext::optional<Error>& error, bool dry_run) {
  if (!running_command_ && !error && activity_log_->level_ != Level::All) {
    return;
  }

  EnsureCreateSystemCommandNodeSignaled();

  std::ostringstream oss;
  activity_log_->OutputNodeHeader(&oss, node_id_, "ProcessingComplete");
  if (dry_run) {
    OutputString(&oss, "dry_run", "true");
  }
  if (error) {
    OutputRawString(&oss, "error", error->str().c_str());
  }
  OutputNodeFooter(&oss);

  activity_log_->Log(oss.str());
}

void ActivityLog::FileProcessNodeLog::EnsureCreateSystemCommandNodeSignaled() {
  if (node_id_ != -1) {
    return;
  }

  node_id_ = activity_log_->CreateNodeId();

  std::ostringstream oss;
  activity_log_->OutputNodeHeader(&oss, node_id_, "CreateSystemCommandNode");

  OutputString(&oss, "command", params_.command);
  OutputPathList(&oss, "inputs", params_.inputs);
  OutputPathList(&oss, "outputs", params_.outputs);
  OutputPathList(&oss, "soft_outs", params_.soft_outputs);
  MaybeOutputPath(&oss, "deps", params_.deps_file);
  MaybeOutputPath(&oss, "stdout", params_.stdout_file);
  MaybeOutputPath(&oss, "stderr", params_.stderr_file);
  MaybeOutputPath(&oss, "stdin", params_.stdin_file);

  OutputNodeFooter(&oss);

  activity_log_->Log(oss.str());
}

ActivityLog::RegistryNodeLog::RegistryNodeLog(
    ActivityLog* activity_log,
    const ebb::lib::JSONPathStringView& path) :
    activity_log_(activity_log),
    path_(path) {
  if (activity_log_->level_ != Level::All) {
    return;
  }

  EnsureCreateRegistryNodeSignaled();
}

ActivityLog::RegistryNodeLog::~RegistryNodeLog() {}

void ActivityLog::RegistryNodeLog::SignalStartDependencyScan() {
  if (activity_log_->level_ != Level::All) {
    return;
  }

  std::ostringstream oss;
  activity_log_->OutputNodeHeader(&oss, node_id_, "ScanningDependencies");
  OutputNodeFooter(&oss);

  activity_log_->Log(oss.str());
}

void ActivityLog::RegistryNodeLog::SignalStartParsingRegistryFile() {
  if (activity_log_->level_ != Level::All) {
    return;
  }

  std::ostringstream oss;
  activity_log_->OutputNodeHeader(&oss, node_id_, "ParsingStarting");
  OutputNodeFooter(&oss);

  activity_log_->Log(oss.str());
}

void ActivityLog::RegistryNodeLog::SignalProcessingComplete(
    const stdext::optional<Error>& error) {
  if (!error && activity_log_->level_ != Level::All) {
    return;
  }
  
  EnsureCreateRegistryNodeSignaled();

  std::ostringstream oss;
  activity_log_->OutputNodeHeader(&oss, node_id_, "ProcessingComplete");
  if (error) {
    OutputRawString(&oss, "error", error->str().c_str());
  }
  OutputNodeFooter(&oss);

  activity_log_->Log(oss.str());
}

void ActivityLog::RegistryNodeLog::EnsureCreateRegistryNodeSignaled() {
  if (node_id_ != -1) {
    return;
  }

  node_id_ = activity_log_->CreateNodeId();

  std::ostringstream oss;
  activity_log_->OutputNodeHeader(&oss, node_id_, "CreateRegistryNode");
  OutputString(&oss, "path", path_);
  OutputNodeFooter(&oss);

  activity_log_->Log(oss.str());
}

void ActivityLog::SignalRespireError(const Error& error) {
  std::ostringstream oss;
  OutputNodeHeader(&oss, -1, "SignalRespireError");
  OutputRawString(&oss, "error", error.str().c_str());
  OutputNodeFooter(&oss);
  Log(oss.str());
}

int ActivityLog::CreateNodeId() {
  std::lock_guard<std::mutex> lock(mutex_);
  return next_node_id_++;    
}

void ActivityLog::Log(const std::string& str) {
  if (ostream_) {
    std::lock_guard<std::mutex> lock(mutex_);
    *ostream_ << str << std::flush;
  }
}

}  // namespace respire
