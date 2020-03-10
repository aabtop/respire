#include "registry_node.h"

#include <fstream>

#include "lib/json_tokenizer.h"
#include "registry_parser.h"
#include "registry_processor.h"

namespace respire {

RegistryNode::RegistryNode(Environment* env, FileInfoNodeOutput input,
                           ebb::lib::JSONPathStringView path,
                           LockedNodeStorage* locked_node_storage)
    : env_(env), activity_log_entry_(env->activity_log(), path),
      parent_(nullptr), input_file_info_node_(input), path_(path),
      locked_node_storage_(locked_node_storage),
      was_populate_locked_node_storage_called_(false),
      push_pull_node_(env->ebb_env(),
                      [this](RegistryNode* parent) {
                        return HandleRequest(parent);
                      }) {}

RegistryNode::FuturePtr RegistryNode::PopulateLockedNodeStorage(
    RegistryNode* parent) {
  return FuturePtr(new Future(&push_pull_node_, parent));
}

namespace {
class ScopedSetValue {
 public:
  ScopedSetValue(RegistryNode** variable, RegistryNode* value)
      : variable_(variable), old_value_(*variable) {
    *variable_ = value;
  }
  ~ScopedSetValue() {
    *variable_ = old_value_;
  }

 private:
  RegistryNode** variable_;
  RegistryNode* old_value_;
};
}  // namespace

OptionalError RegistryNode::HandleRequest(RegistryNode* parent) {
  ScopedSetValue scoped_set_parent(&parent_, parent);

  // If we have some results cached already, just return those.
  if (was_populate_locked_node_storage_called_) {
    return results_;
  }
  was_populate_locked_node_storage_called_ = true;

  activity_log_entry_.SignalStartDependencyScan();

  // Otherwise, ensure our input file is available and then setup our parsing
  // pipeline to parse it.
  FileInfoNode::FuturePtr input_future =
      input_file_info_node_.node->GetFileInfo();
  FileOutput* input_file_infos = input_future->GetValue();

  if (input_file_infos->error()) {
    results_.emplace(input_file_infos->error()->str());
    activity_log_entry_.SignalProcessingComplete(*input_file_infos->error());
    return results_;
  }

  activity_log_entry_.SignalStartParsingRegistryFile();

  const FileInfo& file_info =
      (*input_file_infos->value())[input_file_info_node_.index];
  assert(file_info.filename == path_);
  assert(file_info.last_modified_time);

  // Okay, we have our file path now to parse, setup our pipeline to do the
  // parse.
  ebb::QueueWithMemory<OptionalError, 1> output_queue(env_->ebb_env());
  const size_t kDirectiveQueueSize = 8;
  ebb::QueueWithMemory<RegistryParser::ErrorOrDirective, kDirectiveQueueSize>
      directive_queue(env_->ebb_env());
  RegistryProcessor registry_processor(
      env_, locked_node_storage_, this, &directive_queue, &output_queue);

  const size_t kJSONTokenBufferSize  = 16;
  ebb::lib::BatchedQueueWithMemory<
      ebb::lib::JSONTokenizer::ErrorOrTokens, kJSONTokenBufferSize>
          json_token_queue(env_->ebb_env());
  RegistryParser registry_parser(
      env_->ebb_env(), &json_token_queue, &directive_queue);

  const size_t kFileReadByteBufferSize = 1024;
  ebb::lib::BatchedQueueWithMemory<
      ebb::lib::ErrorOrUInts, kFileReadByteBufferSize>
          byte_queue(env_->ebb_env());
  ebb::lib::JSONTokenizer json_tokenizer(
      env_->ebb_env(), &byte_queue, &json_token_queue, true);

  // Load the entire file in to memory, and then parse string pieces out of it.
  {
    std::ifstream in_file(path_.AsString());
    in_file.seekg(0, std::ios::end);
    size_t file_size = static_cast<size_t>(in_file.tellg());
    in_file.seekg(0);
    data_.resize(file_size);
    in_file.read(reinterpret_cast<char*>(data_.data()), data_.size());

    // Feed the entire file in to the pipeline as input.
    size_t data_size = data_.size();
    while (data_size > 0 && data_[data_size - 1] == '\0') {
      --data_size;
    }

    ebb::Push<ebb::lib::ErrorOrUInts> push(
        byte_queue.queue(),
        ebb::lib::ErrorOrUInts(
            stdext::span<uint8_t>(data_.data(), data_size)));
  }

  // Push the "eof" token.
  {
    ebb::Push<ebb::lib::ErrorOrUInts> push(byte_queue.queue(), 0);
  }

  // Now that our pipeline is setup, pull the result out from it.
  ebb::Pull<OptionalError> pull(&output_queue);
  results_ = *pull.data();

  activity_log_entry_.SignalProcessingComplete(stdext::nullopt);

  return results_;
}

bool RegistryNode::RegistryNodeInCallChain(RegistryNode* node) {
  assert(node);

  RegistryNode* traverse_node = this;
  while (true) {
    if (node == traverse_node) {
      return true;
    }

    traverse_node = traverse_node->parent_;
    if (traverse_node == nullptr) {
      return false;
    }
  }
}

std::vector<RegistryNode*> RegistryNode::GetCallChain() {
  std::vector<RegistryNode*> results;
  RegistryNode* traverse_node = this;

  while (traverse_node) {
    results.push_back(traverse_node);
    traverse_node = traverse_node->parent_;
  }

  std::reverse(results.begin(), results.end());
  return results;
}

}  // namespace respire
