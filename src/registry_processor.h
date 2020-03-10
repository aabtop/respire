#ifndef __RESPIRE_REGISTRY_PROCESSOR_H__
#define __RESPIRE_REGISTRY_PROCESSOR_H__

#include <memory>
#include <string>

#include "ebbpp.h"
#include "environment.h"
#include "error.h"
#include "file_info_node.h"
#include "locked_node_storage.h"
#include "registry_parser.h"
#include "stdext/variant.h"
#include "stdext/file_system.h"

namespace respire {

class RegistryNode;

class RegistryProcessor {
 public:
  using Future = respire::Future<OptionalError>;
  using FuturePtr = std::unique_ptr<Future>;

  RegistryProcessor(
      Environment* env,
      LockedNodeStorage* locked_node_storage,
      RegistryNode* parent_registry_node,
      ebb::Queue<RegistryParser::ErrorOrDirective>* input_queue,
      ebb::Queue<OptionalError>* output_queue);

  ~RegistryProcessor();

 private:

  void PushError(Error error);

  void Consume(RegistryParser::ErrorOrDirective&& input);
  void ConsumeIncludeParams(RegistryParser::IncludeParams* include_params);
  void ConsumeSystemCommandParams(
      RegistryParser::SystemCommandParams* system_command_params);
  void ConsumeBuildParams(RegistryParser::BuildParams* build_params);

  bool VerifyValidSystemCommandOutputOrPushError(
      LockedNodeStorage::Access* access,
      ebb::lib::JSONPathStringView path);

  bool WaitForPendingIncludeDirectives();
  bool BuildPendingBuildTargets();

  Environment* env_;

  RegistryNode* parent_registry_node_;

  ebb::Queue<OptionalError>* output_queue_;

  bool result_pushed_;

  std::vector<FuturePtr> pending_include_fetches_;

  // The list of builds that are requested by this include node to be completed.
  std::vector<FileInfoNode::FuturePtr> pending_builds_;
  // Used to keep track of all the targets we are intending to build.
  std::vector<FileInfoNode*> pending_targets_;

  // The global registry path map allows different RegistryNodes to share
  // child registry nodes.
  LockedNodeStorage* locked_node_storage_;

  ebb::Consumer<RegistryParser::ErrorOrDirective> consumer_;
};

}  // namespace respire

#endif  // __RESPIRE_REGISTRY_PROCESSOR_H__
