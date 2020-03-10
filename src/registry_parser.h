#ifndef __RESPIRE_REGISTRY_PARSER__
#define __RESPIRE_REGISTRY_PARSER__

#include <string>
#include <vector>

#include "ebbpp.h"
#include "lib/json_tokenizer.h"
#include "stdext/file_system.h"
#include "stdext/optional.h"
#include "stdext/variant.h"
#include "system_command_node_params.h"

namespace respire {

class RegistryParser {
 public:
  enum Error {
    kErrorSuccess,
    kErrorTokenizer,
    kErrorUnexpectedToken,
    kErrorInvalidDirectiveName,
    kErrorMultiplyDefinedKey,
    kErrorDidNotFindAllExpectedKeys,
  };

  struct IncludeParams {
    IncludeParams(ebb::lib::JSONPathStringView path)
        : path(path) {}
    bool operator==(const IncludeParams& rhs) const {
      return path == rhs.path;
    }
    ebb::lib::JSONPathStringView path;
  };

  using SystemCommandParams = SystemCommandNodeParams;

  struct BuildParams {
    BuildParams(ebb::lib::JSONPathStringView path)
        : path(path) {}
    bool operator==(const BuildParams& rhs) const {
      return path == rhs.path;
    }
    ebb::lib::JSONPathStringView path;
  };

  using Directive =
      stdext::variant<IncludeParams, SystemCommandParams, BuildParams>;
  using ErrorOrDirective = stdext::variant<Error, Directive>;

  RegistryParser(
      ebb::Environment* env,
      ebb::lib::BatchedQueue<ebb::lib::JSONTokenizer::ErrorOrTokens>*
          input_queue,
      ebb::Queue<ErrorOrDirective>* output_queue);

 private:
  using OptionalToken = stdext::optional<ebb::lib::JSONTokenizer::Token>;

  enum DirectiveType {
    kDirectiveTypeInclude,
    kDirectiveTypeSystemCommand,
    kDirectiveTypeBuild,
    kDirectiveTypeInvalid,
  };

  // If there is another token in the input stream, it will be returned.  If
  // instead a signal is sent, then this method will return a disengaged
  // optional, and the signal received will be set in |signal_|.
  OptionalToken GetNextToken();
  template <typename TokenType>
  OptionalToken GetNextTokenType();
  void SetError(Error error);
  void Parse();
  void ParseTopLevelListEntries();
  bool ParseDirectiveObject();
  DirectiveType ParseDirectiveName();
  static DirectiveType ConvertDirectiveTypeString(
      const ebb::lib::JSONStringView& name);
  enum ParseDirectiveResult {
    kParseDirectiveResultError,
    kParseDirectiveResultSuccess,
    kParseDirectiveResultListEnd,
  };
  ParseDirectiveResult ParseIncludeDirective();
  ParseDirectiveResult ParseSystemCommandDirective();
  ParseDirectiveResult ParseBuildDirective();

  stdext::optional<std::vector<ebb::lib::JSONPathStringView>>
      ParsePathList();

  void ProduceIncludePath(ebb::lib::JSONPathStringView path);
  void ProduceSystemCommand(SystemCommandParams&& system_command_params);
  void ProduceBuildPath(ebb::lib::JSONPathStringView path);

  ebb::lib::BatchedQueue<ebb::lib::JSONTokenizer::ErrorOrTokens>* input_queue_;
  ebb::Queue<ErrorOrDirective>* output_queue_;

  stdext::optional<ebb::lib::JSONTokenizer::Error> tokenizer_signal_;
  bool error_;

  stdext::optional<ebb::Pull<ebb::lib::JSONTokenizer::ErrorOrTokens>>
      current_pull_;
  size_t current_token_index_;

  // Internal queue used to kick off the parsing process.
  ebb::ConsumerWithQueue<void, 1> consumer_;
};

}  // namespace respire

#endif  // __RESPIRE_REGISTRY_PARSER__
