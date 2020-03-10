#ifndef __RESPIRE_FILE_EXISTS_NODE_H__
#define __RESPIRE_FILE_EXISTS_NODE_H__

#include <atomic>
#include <string>
#include <vector>

#include <ebbpp.h>

#include "environment.h"
#include "file_info_node.h"
#include "lib/json_string_view.h"
#include "stdext/file_system.h"

namespace respire {

class FileExistsNode : public FileInfoNode {
 public:
  FileExistsNode(
      Environment* env, ebb::lib::JSONPathStringView file_path);

  FileInfoNode::FuturePtr GetFileInfo(bool dry_run = false) override;

  FileOutput ComputeFileInfo();

  void GetOrderedOutputPaths(
        std::vector<ebb::lib::JSONPathStringView>* paths) override {
    // There's only a single output for FIleExistsNodes.
    paths->clear();
    paths->push_back(file_path_);
  }

 private:
  Environment* env_;
  ebb::lib::JSONPathStringView file_path_;

  std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
  stdext::optional<FileOutput> cached_output_;
};

}  // namespace respire

#endif  // __RESPIRE_FILE_EXISTS_NODE_H__
