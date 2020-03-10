#include "file_exists_node.h"

#include <iostream>
#include <algorithm>

#include "stdext/file_system.h"
#include "stdext/variant.h"

using stdext::file_system::GetLastModificationTime;

namespace respire {

FileExistsNode::FileExistsNode(
    Environment* env, ebb::lib::JSONPathStringView file_path)
    : env_(env), file_path_(file_path) {}

FileInfoNode::FuturePtr FileExistsNode::GetFileInfo(bool dry_run) {
  // Spin to acquire the lock for the cached results.
  while (lock_.test_and_set(std::memory_order_acquire)) {};

  if (!cached_output_) {
    cached_output_ = ComputeFileInfo();
  }

  lock_.clear(std::memory_order_release);

  return FileInfoNode::FuturePtr(new FileInfoNode::Future(*cached_output_));
}

FileOutput FileExistsNode::ComputeFileInfo() {
  stdext::optional<std::chrono::system_clock::time_point> last_modified =
      GetLastModificationTime(file_path_.AsPath());

  if (last_modified.has_value()) {
    return FileOutput(file_path_, *last_modified, false);
  } else {
    std::string error_message =
        "Error: File not found: " + file_path_.AsString();
    Error error(error_message);
    return Error(error);
  }
}

}  // namespace respire
