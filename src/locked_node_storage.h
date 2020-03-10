#ifndef __RESPIRE_LOCKED_NODE_STORAGE_H__
#define __RESPIRE_LOCKED_NODE_STORAGE_H__

#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "environment.h"
#include "stdext/file_system.h"
#include "lib/json_string_view.h"

// This class defines types that are used by RegistryNodes to keep track of
// the nodes that have been parsed.  These collections are shared by the
// root RegistryNode and all of its descendants, and therefore access is guarded
// by a mutex.
namespace respire {

class FileInfoNode;
class RegistryNode;

using FileInfoNodeMap = std::unordered_map<ebb::lib::JSONPathStringView,
                                           FileInfoNodeOutput>;
using FileInfoNodeVector = std::vector<std::unique_ptr<FileInfoNode>>;

using RegistryPathMap = std::unordered_map<ebb::lib::JSONPathStringView,
                                           RegistryNode*>;
using RegistryPathVector = std::vector<std::unique_ptr<RegistryNode>>;

using FileExistsNodes = std::unordered_set<FileInfoNode*>;

class LockedNodeStorage {
 public:
  class Access {
   public:
    Access(LockedNodeStorage* locked_node_storage)
        : lock_(locked_node_storage->mutex_),
          locked_node_storage_(locked_node_storage) {}

    const RegistryPathMap& registry_path_map() const {
      return locked_node_storage_->registry_path_map_;
    }

    RegistryNode* AddRegistryNode(ebb::lib::JSONPathStringView&& path,
                                  std::unique_ptr<RegistryNode> node);

    FileInfoNode* AddFileInfoNode(std::unique_ptr<FileInfoNode> node);

    const FileInfoNodeMap& file_info_node_map() const {
      return locked_node_storage_->file_info_node_map_;
    }

    FileInfoNodeOutput LookupNodeOrMakeFileExistsNode(
        Environment* env,
        ebb::lib::JSONPathStringView path);

    bool IsFileExistsNode(FileInfoNode* node) {
      return locked_node_storage_->file_exists_nodes_.find(node) !=
                 locked_node_storage_->file_exists_nodes_.end();
    }

    ebb::lib::JSONPathStringView AddPathString(
        const stdext::file_system::Path& path) {
      locked_node_storage_->path_string_storage_.emplace_back(
          ebb::lib::ToJSON(path.str()));
      return ebb::lib::JSONPathStringView(
          locked_node_storage_->path_string_storage_.back());
    }

   private:
    std::lock_guard<std::mutex> lock_;
    LockedNodeStorage* locked_node_storage_;
  };

  ~LockedNodeStorage();

 private:
  std::mutex mutex_;

  // We own the nodes inside of vectors, so that we can maintain their
  // construction order and ensure that they are destructed in the correct
  // order.
  FileInfoNodeVector file_info_node_vector_;
  RegistryPathVector registry_path_vector_;

  FileInfoNodeMap file_info_node_map_;
  RegistryPathMap registry_path_map_;

  // Keep track of which files we are assuming exist already and are not
  // generated.
  FileExistsNodes file_exists_nodes_;

  // This stores path string memory for paths that come from places other than
  // JSON files, for example from deps files instead.
  std::vector<std::string> path_string_storage_;

  // Scratch space populated when querying FileInfoNodes for their file output
  // indices.  We reuse it to avoid allocations.
  std::vector<ebb::lib::JSONPathStringView> scratch_output_files_;
};


}  // namespace respire

#endif  // __RESPIRE_LOCKED_NODE_STORAGE_H__
