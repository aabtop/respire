#ifndef __RESPIRE_REGISTRY_NODE_H__
#define __RESPIRE_REGISTRY_NODE_H__

#include <memory>
#include <unordered_map>

#include "environment.h"
#include "file_info_node.h"
#include "future.h"
#include "locked_node_storage.h"
#include "registry_processor.h"
#include "stdext/file_system.h"

namespace respire {

class RegistryNode {
 public:
  // While RegistryNode is the public interface, internally it sets up a
  // pipeline of consumers and the final result is produced by the
  // RegistryProcessor consumer.  Therefore, the final output of the
  // RegistryProcessor is essentially also the output of RegistryNode, hence
  // we forward many of the RegistryProcessor types here.
  using Future = RegistryProcessor::Future;
  using FuturePtr = RegistryProcessor::FuturePtr;

  // Internal constructor is used for creating child RegistryNodes who all
  // share the root node's LockedRegistryPathMap object.
  RegistryNode(Environment* env, FileInfoNodeOutput input,
               ebb::lib::JSONPathStringView path,
               LockedNodeStorage* locked_node_storage);

  FuturePtr PopulateLockedNodeStorage(RegistryNode* parent);

  bool RegistryNodeInCallChain(RegistryNode* node);
  // More general, but more heavy-weight (since it allocates in a std::vector)
  // than the RegistryNodeInCallChain() function above.
  std::vector<RegistryNode*> GetCallChain();

  ebb::lib::JSONPathStringView path() const { return path_; }

 private:
  OptionalError HandleRequest(RegistryNode* parent);

  Environment* env_;

  // The raw input data from the input file.
  std::vector<uint8_t> data_;

  // For logging the activity performed by this node.
  ActivityLog::RegistryNodeLog activity_log_entry_;

  // Temporarily set for the duration of a call to HandleRequest() to be
  // the RegistryNode that initiated that call (or nullptr if it's the root).
  // It is safe for a child to access its parent's |parent_| field, since it
  // is guaranteed that its parent's |parent_| field won't change until its
  // children have all finished executing.
  RegistryNode* parent_;

  FileInfoNodeOutput input_file_info_node_;

  const ebb::lib::JSONPathStringView path_;

  LockedNodeStorage* locked_node_storage_;

  OptionalError results_;

  bool was_populate_locked_node_storage_called_;

  ebb::PushPullConsumer<OptionalError(RegistryNode*)> push_pull_node_;
};

}  // namespace respire

#endif  // __RESPIRE_REGISTRY_NODE_H__