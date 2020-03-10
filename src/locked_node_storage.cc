#include "locked_node_storage.h"

#include "file_info_node.h"
#include "registry_node.h"
#include "file_exists_node.h"
namespace respire {

RegistryNode* LockedNodeStorage::Access::AddRegistryNode(
    ebb::lib::JSONPathStringView&& path,
    std::unique_ptr<RegistryNode> node) {
  assert(registry_path_map().find(path) == registry_path_map().end());
  RegistryNode* node_pointer = node.get();
  locked_node_storage_->registry_path_vector_.emplace_back(std::move(node));
  locked_node_storage_->registry_path_map_.emplace(
      std::move(path), node_pointer);
  return node_pointer;
}

FileInfoNode* LockedNodeStorage::Access::AddFileInfoNode(
    std::unique_ptr<FileInfoNode> node) {
  FileInfoNode* node_pointer = node.get();
  locked_node_storage_->file_info_node_vector_.emplace_back(std::move(node));

  auto& scratch_output_files = locked_node_storage_->scratch_output_files_;
  scratch_output_files.clear();
  node_pointer->GetOrderedOutputPaths(&scratch_output_files);

  for (size_t i = 0; i < scratch_output_files.size(); ++i) {
    const auto& path = scratch_output_files[i];

    assert(file_info_node_map().find(path) == file_info_node_map().end());
    locked_node_storage_->file_info_node_map_.emplace(
        path, FileInfoNodeOutput(node_pointer, i));
  }

  return node_pointer;
}

FileInfoNodeOutput LockedNodeStorage::Access::LookupNodeOrMakeFileExistsNode(
    Environment* env, ebb::lib::JSONPathStringView path) {
  auto found = file_info_node_map().find(path);
  if (found == file_info_node_map().end()) {
    // If the node is not in the node map already, then create a new
    // "file exists node" for this path and add that instead.
    auto node = std::unique_ptr<FileExistsNode>(new FileExistsNode(env, path));
    locked_node_storage_->file_exists_nodes_.insert(node.get());
    return FileInfoNodeOutput(AddFileInfoNode(std::move(node)), 0);
  } else {
    return found->second;
  }
}

LockedNodeStorage::~LockedNodeStorage() {
  do {
    size_t previous_size;
    std::vector<RegistryNode::FuturePtr> futures;

    mutex_.lock();

    previous_size = registry_path_vector_.size();
    futures.reserve(previous_size);

    for (size_t i = 0; i < previous_size; ++i) {
      RegistryNode* node = registry_path_vector_[i].get();

      mutex_.unlock();
      futures.emplace_back(node->PopulateLockedNodeStorage(nullptr));
      mutex_.lock();
    }

    mutex_.unlock();

    for (auto& future : futures) {
      future->GetValue();
    }

    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (previous_size == registry_path_vector_.size()) {
        break;
      }
    }
  } while (true);

  // Ensure that the vector elements are destructed in reverse order from their
  // construction (which we assume coincides with their insertion into the
  // vector).
  while (!registry_path_vector_.empty()) {
    registry_path_vector_.pop_back();
  }

  // We destruct the file info nodes after the registry nodes because it is
  // possible for registry nodes to depend on file info nodes, but not
  // vice-versa.
  while (!file_info_node_vector_.empty()) {
    file_info_node_vector_.pop_back();
  }
}

}  // namespace respire
