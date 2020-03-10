#include "file_process_node.h"

#include <algorithm>
#include <chrono>

#include "stdext/file_system.h"

using stdext::optional;
using std::chrono::system_clock;

namespace respire {

FileProcessNode::FileProcessNode(
    Environment* env,
    const std::vector<FileInfoNodeOutput>&& inputs,
    const std::vector<ebb::lib::JSONPathStringView>* output_files,
    const std::vector<ebb::lib::JSONPathStringView>* soft_output_files,
    const std::function<optional<Error>()>& command,
    GetDepsFunction get_deps_function,
    ActivityLog::FileProcessNodeLog* activity_log_entry)
    : activity_log_entry_(activity_log_entry), inputs_(std::move(inputs)),
      output_files_(output_files), soft_output_files_(soft_output_files),
      command_(command), get_deps_function_(get_deps_function),
      push_pull_node_(
          env->ebb_env(),
          [this](bool dry_run) { return HandleRequest(dry_run); }) {}

FileProcessNode::FileProcessNode(
    Environment* env,
    const std::vector<FileInfoNodeOutput>&& inputs,
    const std::vector<ebb::lib::JSONPathStringView>&& output_files,
    const std::vector<ebb::lib::JSONPathStringView>&& soft_output_files,
    const std::function<optional<Error>()>& command,
    GetDepsFunction get_deps_function,
    ActivityLog::FileProcessNodeLog* activity_log_entry)
    : activity_log_entry_(activity_log_entry), inputs_(std::move(inputs)),
      owned_output_files_(std::move(output_files)),
      output_files_(&owned_output_files_.value()),
      owned_soft_output_files_(std::move(soft_output_files)),
      soft_output_files_(&owned_soft_output_files_.value()),
      command_(command), get_deps_function_(get_deps_function),
      push_pull_node_(
          env->ebb_env(),
          [this](bool dry_run) { return HandleRequest(dry_run); }) {}

FileInfoNode::FuturePtr FileProcessNode::GetFileInfo(bool dry_run) {
  return FileInfoNode::FuturePtr(
      new FileInfoNode::Future(&push_pull_node_, dry_run));
}

void FileProcessNode::GetOrderedOutputPaths(
    std::vector<ebb::lib::JSONPathStringView>* paths) {
  paths->clear();
  for (const auto& output_file : *output_files_) {
    paths->push_back(output_file);
  }
  for (const auto& soft_output_file : *soft_output_files_) {
    paths->push_back(soft_output_file);
  }  
}

namespace {
std::vector<optional<system_clock::time_point>> GetLastModificationTimes(
    const std::vector<ebb::lib::JSONPathStringView>& files) {
  std::vector<optional<system_clock::time_point>> times;
  times.reserve(files.size());
  for (const auto& file : files) {
    times.emplace_back(GetLastModificationTime(file.AsPath()));
  }
  return times;
}

bool AnyInputNewerThanOutputs(
    const std::vector<FileInfoNode::FuturePtr>& input_futures,
    const std::vector<FileInfoNodeOutput>& inputs,
    const std::vector<optional<system_clock::time_point>> output_times,
    bool newer_or_equal_to) {
  // Of all the output files, determine their oldest modification time.
  system_clock::time_point oldest_output_time = system_clock::time_point::max();
  for (const auto& output_time : output_times) {
    if (!output_time.has_value()) {
      // If any of the output files don't exist, unconditionally return true.
      return true;
    }

    if (*output_time < oldest_output_time) {
      oldest_output_time = *output_time;
    }
  }

  // If any of the input file modification times are later than the oldest
  // output file modification time, return true.
  for (size_t i = 0; i < input_futures.size(); ++i) {
    const FileInfo& file_info =
        (*input_futures[i]->GetValue()->value())[inputs[i].index];

    if (!file_info.last_modified_time) {
      return true;
    }
    if (newer_or_equal_to) {
      if (*file_info.last_modified_time >= oldest_output_time) {
        return true;
      }
    } else {
      if (*file_info.last_modified_time > oldest_output_time) {
        return true;
      }
    }
  }

  return false;
} 

}  // namespace

FileProcessNode::ComputeFileOutputResult FileProcessNode::ComputeFileOutput(
    bool dry_run) {
  if (activity_log_entry_) {
    activity_log_entry_->SignalStartDependencyScan(dry_run);
  }

  // Ensure that the inputs are up-to-date before running the command.
  std::vector<FileInfoNode::FuturePtr> input_futures;
  input_futures.reserve(inputs_.size());
  for (const auto& input : inputs_) {
    input_futures.emplace_back(input.node->GetFileInfo(dry_run));
  }

  // Now that requests have been sent out to the inputs, while we wait for them
  // we will look up the last modified times of our output files.
  std::vector<optional<system_clock::time_point>> output_times =
      GetLastModificationTimes(*output_files_);

  // Now join on all input nodes to ensure they are created.  If there were
  // any errors in the inputs, propagate them.
  for (const auto& input_future : input_futures) {
    const FileOutput& input_result = *input_future->GetValue();
    if (input_result.error()) {
      // Propagate any errors from the input nodes if there are some.
      LogProcessingComplete(*input_result.error(), dry_run);
      return ComputeFileOutputResult(input_result);
    }
  }

  bool should_rebuild = AnyInputNewerThanOutputs(
      input_futures, inputs_, output_times, true);
  if (!should_rebuild && get_deps_function_) {
    // Looks like none of the specified inputs were modified, check to see if
    // our deps function has any additional dependencies for us to check (e.g.
    // header files referenced by a C++ source file).
    stdext::optional<std::vector<FileInfoNodeOutput>> extra_deps =
        get_deps_function_();
    if (!extra_deps) {
      // If there was an error reading the extra deps, assume the worst (e.g.
      // a deps file was deleted) and just rebuild everything.
      should_rebuild = true;
    } else {
      // Read through the additional dependencies and check their modification
      // dates also.
      std::vector<FileInfoNode::FuturePtr> deps_futures;
      deps_futures.reserve(extra_deps->size());
      for (const auto& dep : *extra_deps) {
        deps_futures.emplace_back(dep.node->GetFileInfo());
      }
      for (const auto& dep_future : deps_futures) {
        const FileOutput& dep_result = *dep_future->GetValue();
        if (dep_result.error()) {
          // If there were any errors reading the deps files, instead of
          // propagating the error just drop everything and try to rebuild.
          // This course of action makes sense when, for example, a C source
          // file's #included header is renamed, in which case the deps file
          // references a file that no longer exists, but we can and should
          // still successfully build the source file.
          should_rebuild = true;
          break;
        }
      }
      if (!should_rebuild) {
        should_rebuild = AnyInputNewerThanOutputs(
                             deps_futures, *extra_deps, output_times, true);
      }
    }
  }

  bool fake_dry_run_result = false;
  if (should_rebuild) {
    if (activity_log_entry_) {
      activity_log_entry_->SignalStartRunningCommand(dry_run);
    }

    if (!dry_run) {
      stdext::optional<Error> maybe_error = command_();

      if (maybe_error) {
        LogProcessingComplete(maybe_error, dry_run);
        return FileOutput(
            Error("Error executing command: " + maybe_error->str()));
      }

      // Refresh the output times now that they (may) have each been modified.
      output_times = GetLastModificationTimes(*output_files_);

      if (AnyInputNewerThanOutputs(
              input_futures, inputs_, output_times, false)) {
        // If the condition still holds that input files are newer than output
        // files, then return an error.
        std::string error_message(
            "Not all output files were modified by a FileProcessNode(). "
            "If this is what you want, specify 'soft output's instead.");
        Error error(error_message);
        LogProcessingComplete(error, dry_run);
        return FileOutput(std::move(error));
      }
    } else {
      fake_dry_run_result = true;

      // Create fake output modification times.
      auto now = std::chrono::system_clock::now();
      for (auto& time : output_times) {
        time = now;
      }
    }
  }

  // Zip up the output file names with the output modification times and return
  // the result.
  FileOutput::Value ret;
  ret.reserve(output_files_->size() + soft_output_files_->size());
  for (size_t i = 0; i < output_files_->size(); ++i) {
    ret.emplace_back((*output_files_)[i], *output_times[i], false);
  }

  // Now add in the soft output file modification times, which we will need
  // to now look up.
  if (!dry_run || !should_rebuild) {
    for (const auto& soft_output_file : *soft_output_files_) {
      ret.emplace_back(soft_output_file,
                       GetLastModificationTime(soft_output_file.AsPath()),
                       true);
    }
  } else {
    // Populate soft output results with fake modification times.
    fake_dry_run_result = true;

    auto now = std::chrono::system_clock::now();
    for (const auto& soft_output_file : *soft_output_files_) {
      ret.emplace_back(soft_output_file, now, true);
    }
  }

  LogProcessingComplete(stdext::nullopt, dry_run);
  return ComputeFileOutputResult(FileOutput(std::move(ret)),
                                 fake_dry_run_result);
}

FileOutput FileProcessNode::HandleRequest(bool dry_run) {
  if (!cached_output_ || (!dry_run && cached_output_is_fake_dry_run_)) {
    ComputeFileOutputResult results = ComputeFileOutput(dry_run);
    cached_output_.emplace(std::move(results.file_output));
    cached_output_is_fake_dry_run_ = results.fake_dry_run_result;
  }

  return *cached_output_;
}

void FileProcessNode::LogProcessingComplete(
    const stdext::optional<Error>& error, bool dry_run) {
  if (activity_log_entry_) {
    activity_log_entry_->SignalProcessingComplete(error, dry_run);
  }
}

}  // namespace respire
