#ifndef __RESPIRE_FILE_PROCESS_NODE_H__
#define __RESPIRE_FILE_PROCESS_NODE_H__

#include <string>
#include <vector>

#include "ebbpp.h"

#include "environment.h"
#include "file_info_node.h"
#include "lib/json_string_view.h"
#include "stdext/file_system.h"

namespace respire {

// The FileProcessNode takes a list of input file nodes, a list of output
// file paths, and an arbitrary function.  The function will be called and
// is expected to use the input file nodes to generate the set of output
// file paths.
class FileProcessNode : public FileInfoNode {
 public:
  // Returns a vector of additional input dependencies.  Returns a null optional
  // if there was an error loading the additional dependencies, in which case
  // that should be treated the same as one of the dependencies being out of
  // date.
  using GetDepsFunction =
      std::function<stdext::optional<std::vector<FileInfoNodeOutput>>()>;

  FileProcessNode(
      Environment* env,
      const std::vector<FileInfoNodeOutput>&& inputs,
      const std::vector<ebb::lib::JSONPathStringView>* output_files,
      const std::vector<ebb::lib::JSONPathStringView>* soft_output_files,
      const std::function<stdext::optional<Error>()>& command,
      GetDepsFunction get_deps_function = GetDepsFunction(),
      ActivityLog::FileProcessNodeLog* activity_log_entry = nullptr);

  FileProcessNode(
      Environment* env,
      const std::vector<FileInfoNodeOutput>&& inputs,
      const std::vector<ebb::lib::JSONPathStringView>&& output_files,
      const std::vector<ebb::lib::JSONPathStringView>&& soft_output_files,
      const std::function<stdext::optional<Error>()>& command,
      GetDepsFunction get_deps_function = GetDepsFunction(),
      ActivityLog::FileProcessNodeLog* activity_log_entry = nullptr);

  FileInfoNode::FuturePtr GetFileInfo(bool dry_run = false) override;

  void GetOrderedOutputPaths(
      std::vector<ebb::lib::JSONPathStringView>* paths) override;

 private:
  struct ComputeFileOutputResult {
    ComputeFileOutputResult(const FileOutput& file_output)
        : file_output(file_output) {}
    ComputeFileOutputResult(FileOutput&& file_output)
        : file_output(std::move(file_output)) {}
    ComputeFileOutputResult(FileOutput&& file_output, bool fake_dry_run_result)
        : file_output(std::move(file_output)),
          fake_dry_run_result(fake_dry_run_result) {}
    // The actual results.
    FileOutput file_output;
    // If true, indicates that the result is faked because |dry_run| a dry
    // run was requested.  This value could be false even if a |dry_run| is
    // requested if we were able to determine that no file processing was
    // necessary anyway (and so the result is valid despite the fact that we
    // requested a dry run.)
    bool fake_dry_run_result = false;
  };
  // This function assumes that no caching is involved and just always computes
  // the FileOutput result.
  ComputeFileOutputResult ComputeFileOutput(bool dry_run);
  FileOutput HandleRequest(bool dry_run);

  void LogProcessingComplete(const stdext::optional<Error>& error,
                             bool dry_run);

  ActivityLog::FileProcessNodeLog* activity_log_entry_;

  const std::vector<FileInfoNodeOutput> inputs_;

  const stdext::optional<std::vector<ebb::lib::JSONPathStringView>>
      owned_output_files_;
  const std::vector<ebb::lib::JSONPathStringView>* output_files_;
  const stdext::optional<std::vector<ebb::lib::JSONPathStringView>>
      owned_soft_output_files_;
  const std::vector<ebb::lib::JSONPathStringView>* soft_output_files_;
  std::function<stdext::optional<Error>()> command_;
  GetDepsFunction get_deps_function_;

  stdext::optional<FileOutput> cached_output_;

  // Note that PushPullConsumer by default has a queue size of 1, meaning that
  // if more than one entity are waiting on the results of it, only the first
  // one will not block.  This could be an issue if one wishes to kick off
  // a build on multiple targets that depend on the others.
  ebb::PushPullConsumer<FileOutput(bool)> push_pull_node_;

  bool cached_output_is_fake_dry_run_ = false;
};

}  // namespace respire

#endif  // __RESPIRE_FILE_PROCESS_NODE_H__
