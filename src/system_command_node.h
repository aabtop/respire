#ifndef __RESPIRE_SYSTEM_COMMAND_NODE_H__
#define __RESPIRE_SYSTEM_COMMAND_NODE_H__

#include <string>
#include <vector>

#include "activity_log.h"
#include "ebbpp.h"
#include "environment.h"
#include "file_info_node.h"
#include "file_process_node.h"
#include "lib/json_string_view.h"
#include "stdext/file_system.h"
#include "stdext/optional.h"
#include "system_command_node_params.h"

namespace respire {

class SystemCommandNode : public FileInfoNode {
 public:
  SystemCommandNode(
      Environment* env, std::vector<FileInfoNodeOutput>&& inputs,
      SystemCommandNodeParams&& params,
      FileProcessNode::GetDepsFunction get_deps_function =
          FileProcessNode::GetDepsFunction());

  FileInfoNode::FuturePtr GetFileInfo(bool dry_run = false) override {
    return file_process_node_.GetFileInfo(dry_run);
  }

  void GetOrderedOutputPaths(
      std::vector<ebb::lib::JSONPathStringView>* paths) override {
    file_process_node_.GetOrderedOutputPaths(paths);
  }

  const SystemCommandNodeParams& params() const {
    return activity_log_entry_.params();
  }

 private:
  ActivityLog::FileProcessNodeLog activity_log_entry_;
  FileProcessNode file_process_node_;
};

}  // namespace respire

#endif  // __RESPIRE_SYSTEM_COMMAND_NODE_H__
