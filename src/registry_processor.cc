#include "registry_processor.h"

#include <sstream>

#include "parse_deps.h"
#include "registry_node.h"
#include "registry_parser.h"
#include "system_command_node.h"

namespace respire {

RegistryProcessor::RegistryProcessor(
    Environment* env,
    LockedNodeStorage* locked_node_storage,
    RegistryNode* parent_registry_node,
    ebb::Queue<RegistryParser::ErrorOrDirective>* input_queue,
    ebb::Queue<OptionalError>* output_queue)
    : env_(env),
      parent_registry_node_(parent_registry_node),
      output_queue_(output_queue),
      result_pushed_(false),
      locked_node_storage_(locked_node_storage),
      consumer_(
          env->ebb_env(), input_queue,
          [this](RegistryParser::ErrorOrDirective&& input) {
            Consume(std::move(input));
          }) {}

RegistryProcessor::~RegistryProcessor() {}

void RegistryProcessor::PushError(Error error) {
  ebb::Push<OptionalError>(output_queue_, error);
  result_pushed_ = true;
}

void RegistryProcessor::Consume(RegistryParser::ErrorOrDirective&& input) {
  if (result_pushed_) {
    return;
  }

  // If we receive the error/success signal, return either our populated file
  // info map on success, or an error on error.
  if (stdext::holds_alternative<RegistryParser::Error>(input)) {
    RegistryParser::Error error =
        stdext::get<RegistryParser::Error>(input);
    if (error == RegistryParser::kErrorSuccess) {
      if (WaitForPendingIncludeDirectives() &&
          BuildPendingBuildTargets()) {
        // There should not have been any errors pushed up until now.
        assert(!result_pushed_);
        // Push a null optional, implying success.
        ebb::Push<OptionalError>(output_queue_, stdext::nullopt);
        result_pushed_ = true;
      }
    } else {
      PushError(Error("Parser error."));
    }

    return;
  }

  // Otherwise, let's process this incoming directive!
  RegistryParser::Directive* directive =
      &stdext::get<RegistryParser::Directive>(input);

  if (stdext::holds_alternative<
          RegistryParser::IncludeParams>(*directive)) {
    ConsumeIncludeParams(
        &stdext::get<RegistryParser::IncludeParams>(*directive));
  } else if (stdext::holds_alternative<
                 RegistryParser::SystemCommandParams>(*directive)) {
    ConsumeSystemCommandParams(
        &stdext::get<RegistryParser::SystemCommandParams>(*directive));
  } else if (stdext::holds_alternative<RegistryParser::BuildParams>(
                 *directive)) {
    ConsumeBuildParams(
        &stdext::get<RegistryParser::BuildParams>(*directive));
  } else {
    assert(false);
  }
}

namespace {
std::string GenerateCyclicDependencyErrorMessage(
    RegistryNode* error_node, RegistryNode* parent_node) {
  std::vector<RegistryNode*> call_chain(parent_node->GetCallChain());

  std::ostringstream oss;
  oss << "Cyclic dependency detected:" << std::endl;

  bool found_error_node = false;
  for (const auto& node : call_chain) {
    if (node == error_node) {
      // We can't find the error node in the callstack twice, otherwise there
      // was a previous cyclic dependency that went undetected.
      assert(!found_error_node);
      found_error_node = true;
    }
    if (found_error_node) {
      oss << node->path().AsString() << std::endl;
      oss << "->" << std::endl;
    }
  }

  oss << error_node->path().AsString() << std::endl;

  return oss.str();
}
}  // namespace

void RegistryProcessor::ConsumeIncludeParams(
    RegistryParser::IncludeParams* include_params) {
  // Add or acquire the node from the global registry node map.
  RegistryNode* registry_node = nullptr;
  {
    LockedNodeStorage::Access access(locked_node_storage_);
    const RegistryPathMap& registry_path_map = access.registry_path_map();

    auto found = registry_path_map.find(include_params->path);
    if (found == registry_path_map.end()) {
      // Create a new RegistryNode and add it to the registry.
      FileInfoNodeOutput input_node = access.LookupNodeOrMakeFileExistsNode(
          env_, include_params->path);
      std::unique_ptr<RegistryNode> node(new RegistryNode(
          env_, input_node, include_params->path,
          locked_node_storage_));
      registry_node =
          access.AddRegistryNode(std::move(include_params->path), std::move(node));
    } else {
      registry_node = found->second;

      if (parent_registry_node_->RegistryNodeInCallChain(registry_node)) {
        PushError(GenerateCyclicDependencyErrorMessage(
                      registry_node, parent_registry_node_));
        return;
      }
    }
  }

  // We kick off the include work here, but we don't wait on it to complete
  // until we need it, e.g. when a system command directive comes in, or when
  // we run out of directives to process.
  pending_include_fetches_.emplace_back(
      registry_node->PopulateLockedNodeStorage(parent_registry_node_));
}

namespace {

size_t NumInputsInSystemCommand(
    const RegistryParser::SystemCommandParams& params) {
  return params.inputs.size() + (params.stdin_file ? 1 : 0);
}

}  // namespace

void RegistryProcessor::ConsumeSystemCommandParams(
    RegistryParser::SystemCommandParams* system_command_params) {
  // Before doing any processing that relies on the state of
  // |output_.file_info_node_map|, make sure that we have finished resolving any
  // prior pending include directives.
  if (!WaitForPendingIncludeDirectives()) {
    // If there was an error processing prior include directives, we're done.
    return;
  }

  // Create the set of all input nodes.  It is an error for them to not be
  // previously defined in the registry.
  std::vector<FileInfoNodeOutput> inputs;
  inputs.reserve(NumInputsInSystemCommand(*system_command_params));

  if (system_command_params->stdout_file) {
    system_command_params->outputs.push_back(
        *system_command_params->stdout_file);
  }
  if (system_command_params->stderr_file) {
    system_command_params->outputs.push_back(
        *system_command_params->stderr_file);
  }

  LockedNodeStorage::Access access(locked_node_storage_);

  for (const auto& output : system_command_params->outputs) {
    if (!VerifyValidSystemCommandOutputOrPushError(&access, output)) {
      return;
    }
  }
  for (const auto& output : system_command_params->soft_outputs) {
    if (!VerifyValidSystemCommandOutputOrPushError(&access, output)) {
      return;
    }
  }

  for (const auto& input : system_command_params->inputs) {
    FileInfoNodeOutput input_node = access.LookupNodeOrMakeFileExistsNode(
        env_, input);
    inputs.push_back(input_node);
  }

  if (system_command_params->stdin_file) {
    FileInfoNodeOutput stdin_node = access.LookupNodeOrMakeFileExistsNode(
        env_, *system_command_params->stdin_file);
    inputs.push_back(stdin_node);
  }

  FileProcessNode::GetDepsFunction get_deps_function;
  if (system_command_params->deps_file) {
    FileInfoNodeOutput deps_node = access.LookupNodeOrMakeFileExistsNode(
        env_, *system_command_params->deps_file);
    get_deps_function =
        std::bind(&ParseDeps, env_, locked_node_storage_, deps_node,
                  *system_command_params->deps_file);
  }

  // Construct the system command node.
  auto node = std::unique_ptr<SystemCommandNode>(
      new SystemCommandNode(
          env_, std::move(inputs), std::move(*system_command_params),
          get_deps_function));

  // Add the new system command node to the registry, with a key for each of the
  // outputs that it produces.
  access.AddFileInfoNode(std::move(node));
}

void RegistryProcessor::ConsumeBuildParams(
    RegistryParser::BuildParams* build_params) {
  // Since build directives rely on |output_.file_info_node_map|, make sure that
  // we have finished resolving any prior pending include directives.
  if (!WaitForPendingIncludeDirectives()) {
    // If there was an error processing prior include directives, we're done.
    return;
  }

  FileInfoNodeOutput target_output;
  {
    LockedNodeStorage::Access access(locked_node_storage_);
    const FileInfoNodeMap& file_node_map = access.file_info_node_map();
    auto found = file_node_map.find(build_params->path);
    if (found == file_node_map.end()) {
      std::ostringstream oss;
      oss << "Target not specified as an output in registry files:";
      oss << std::endl << build_params->path.AsString() << std::endl;
      PushError(oss.str());
      return;
    }
    target_output = found->second;
  }

  // Start a dry run on our build targets to induce log output that can be
  // used to determine the number of nodes we will be building.  In this
  // RegistryProcessor's destructor, we will initiate the actual fetches and
  // wait for them to resolve.
  pending_builds_.emplace_back(target_output.node->GetFileInfo(true));
  pending_targets_.emplace_back(target_output.node);
}

bool RegistryProcessor::VerifyValidSystemCommandOutputOrPushError(
    LockedNodeStorage::Access* access,
    ebb::lib::JSONPathStringView output_path) {
  // Make sure that none of the outputs have been previously specified by other
  // nodes.
  auto found = access->file_info_node_map().find(output_path);
  if (found != access->file_info_node_map().end()) {
    // Enter error mode and stop.
    if (access->IsFileExistsNode(found->second.node)) {
      std::ostringstream oss;
      oss << "Path was referenced as an input before a system command "
          << "was defined which referenced it as an output." << std::endl;
      oss << "Path: " << output_path.AsString() << std::endl;
      PushError(oss.str());
    } else {
      std::ostringstream oss;
      oss << "Output path specified more than once:" << std::endl;
      oss << output_path.AsString() << std::endl;
      PushError(oss.str());
    }
    return false;
  }
  return true;
}

bool RegistryProcessor::WaitForPendingIncludeDirectives() {
  for (auto& future : pending_include_fetches_) {
    OptionalError* maybe_error = future->GetValue();
    // First check if there was an error with the sub-registry parsing.
    if (*maybe_error) {
      PushError(**maybe_error);
      return false;
    }
  }

  return true;
}

bool RegistryProcessor::BuildPendingBuildTargets() {
  for (auto& target_node : pending_targets_) {
    // Up until now we have only kicked off dry runs...  Now initiate the
    // real thing.
    pending_builds_.emplace_back(target_node->GetFileInfo(false));
  }

  for (auto& pending_build : pending_builds_) {
    const FileOutput* output = pending_build->GetValue();
    if (output->error()) {
      PushError(*output->error());
      return false;
    }
  }

  return true;
}

}  // namespace respire
