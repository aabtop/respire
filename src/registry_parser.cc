#include "registry_parser.h"

#include <iostream>

using ebb::lib::JSONTokenizer;

namespace respire {

RegistryParser::RegistryParser(
      ebb::Environment* env,
      ebb::lib::BatchedQueue<ebb::lib::JSONTokenizer::ErrorOrTokens>*
          input_queue,
      ebb::Queue<ErrorOrDirective>* output_queue)
    : input_queue_(input_queue),
      output_queue_(output_queue),
      error_(false),
      consumer_(env, std::bind(&RegistryParser::Parse, this)) {
  ebb::Push<>(consumer_.queue());
}

RegistryParser::OptionalToken RegistryParser::GetNextToken() {
  // GetNextToken() should never be called after an error or success is
  // signaled.
  assert(!tokenizer_signal_.has_value());

  if (!current_pull_.has_value()) {
    current_pull_.emplace(input_queue_->queue());

    if (stdext::holds_alternative<JSONTokenizer::Error>(
            *current_pull_->data())) {
      tokenizer_signal_ =
          stdext::get<JSONTokenizer::Error>(*current_pull_->data());
      if (!error_ && *tokenizer_signal_ != JSONTokenizer::kSuccess) {
        SetError(kErrorTokenizer);
      }
      return stdext::nullopt;
    }

    current_token_index_ = 0;
  }

  stdext::span<JSONTokenizer::Token>* tokens =
      &stdext::get<stdext::span<JSONTokenizer::Token>>(
           *current_pull_->data());

  OptionalToken ret(std::move(tokens->data()[current_token_index_]));

  ++current_token_index_;
  if (current_token_index_ == tokens->size()) {
    current_pull_.reset();
  }

  return std::move(ret);
}


template <typename TokenType>
RegistryParser::OptionalToken RegistryParser::GetNextTokenType() {
  OptionalToken token = GetNextToken();
  if (!token) {
    return stdext::nullopt;
  }
  if (!stdext::holds_alternative<TokenType>(*token)) {
    SetError(kErrorUnexpectedToken);
    return stdext::nullopt;
  }
  return std::move(token);
}

void RegistryParser::SetError(Error error) {
  assert(!error_);
  error_ = true;
  ebb::Push<ErrorOrDirective>(output_queue_, error);
}

void RegistryParser::Parse() {
  // First read the list opening brace out of the way.
  OptionalToken open_list = GetNextTokenType<JSONTokenizer::StartListToken>();
  if (!open_list) {
    return;
  } else {
    ParseTopLevelListEntries();

    // Drain the rest of the tokens, though hopefully there are none and there
    // is just the success signal waiting for us.
    while (!tokenizer_signal_) {
      OptionalToken drain_token = GetNextToken();
      if (drain_token && !error_) {
        SetError(kErrorUnexpectedToken);
      }
    }
  }

  if (!error_) {
    ebb::Push<ErrorOrDirective>(output_queue_, kErrorSuccess);
  }
}

void RegistryParser::ParseTopLevelListEntries() {
  do {
    OptionalToken cur_token = GetNextToken();
    if (!cur_token) {
      // A parse error occurred, so just return.
      return;
    }

    if (stdext::holds_alternative<JSONTokenizer::StartObjectToken>(
            *cur_token)) {
      if (!ParseDirectiveObject()) {
        return;
      }
    } else if (stdext::holds_alternative<JSONTokenizer::EndListToken>(
                   *cur_token)) {
      // The top-level list has closed, we're done parsing, return.
      return;
    } else {
      SetError(kErrorUnexpectedToken);
      return;
    }
  } while(true);
}

bool RegistryParser::ParseDirectiveObject() {
  DirectiveType directive_type = ParseDirectiveName();

  // Parse the opening list token.
  if (!GetNextTokenType<JSONTokenizer::StartListToken>()) {
    return false;
  }

  // Parse the list entries.
  do {
    ParseDirectiveResult result;
    if (directive_type == kDirectiveTypeInclude) {
      result = ParseIncludeDirective();
    } else if (directive_type == kDirectiveTypeSystemCommand) {
      result = ParseSystemCommandDirective();
    } else if (directive_type == kDirectiveTypeBuild) {
      result = ParseBuildDirective();
    }

    if (result == kParseDirectiveResultError) {
      return false;
    } else if (result == kParseDirectiveResultListEnd) {
      break;
    }
  } while(true);

  // Parse the closing object token.
  if (!GetNextTokenType<JSONTokenizer::EndObjectToken>()) {
    return false;
  }

  return true;
}

RegistryParser::DirectiveType RegistryParser::ParseDirectiveName() {
  OptionalToken directive_token =
      GetNextTokenType<JSONTokenizer::JSONStringViewToken>();
  if (!directive_token) {
    return kDirectiveTypeInvalid;
  }

  const JSONTokenizer::JSONStringViewToken& directive_name =
      stdext::get<JSONTokenizer::JSONStringViewToken>(*directive_token);
  DirectiveType directive_type = ConvertDirectiveTypeString(
      directive_name.string_view);
  if (directive_type == kDirectiveTypeInvalid) {
    SetError(kErrorInvalidDirectiveName);
    return kDirectiveTypeInvalid;    
  }

  return directive_type;
}

// static
RegistryParser::DirectiveType RegistryParser::ConvertDirectiveTypeString(
    const ebb::lib::JSONStringView& name) {
  if (name.IsEqual("inc")) {
    return kDirectiveTypeInclude;
  } else if (name.IsEqual("sc")) {
    return kDirectiveTypeSystemCommand;
  } else if (name.IsEqual("build")) {
    return kDirectiveTypeBuild;
  } else {
    return kDirectiveTypeInvalid;
  }
}

RegistryParser::ParseDirectiveResult RegistryParser::ParseIncludeDirective() {
  OptionalToken token = GetNextToken();
  if (!token) {
    return kParseDirectiveResultError;
  }
  if (stdext::holds_alternative<JSONTokenizer::EndListToken>(*token)) {
    return kParseDirectiveResultListEnd;
  }
  if (!stdext::holds_alternative<JSONTokenizer::JSONStringViewToken>(
           *token)) {
    SetError(kErrorUnexpectedToken);
    return kParseDirectiveResultError;
  }

  ProduceIncludePath(
      stdext::get<JSONTokenizer::JSONStringViewToken>(*token).string_view);

  return kParseDirectiveResultSuccess;
}

RegistryParser::ParseDirectiveResult
RegistryParser::ParseSystemCommandDirective() {
  OptionalToken token = GetNextToken();
  if (!token) {
    return kParseDirectiveResultError;
  }
  if (stdext::holds_alternative<JSONTokenizer::EndListToken>(*token)) {
    return kParseDirectiveResultListEnd;
  }
  if (!stdext::holds_alternative<JSONTokenizer::StartObjectToken>(
           *token)) {
    SetError(kErrorUnexpectedToken);
    return kParseDirectiveResultError;
  }

  stdext::optional<ebb::lib::JSONStringView> command_param;
  stdext::optional<std::vector<ebb::lib::JSONPathStringView>>
      inputs_param;
  stdext::optional<std::vector<ebb::lib::JSONPathStringView>>
      outputs_param;
  stdext::optional<std::vector<ebb::lib::JSONPathStringView>>
      soft_outputs_param;
  stdext::optional<ebb::lib::JSONPathStringView> deps_param;
  stdext::optional<ebb::lib::JSONPathStringView> stdout_param;
  stdext::optional<ebb::lib::JSONPathStringView> stderr_param;
  stdext::optional<ebb::lib::JSONPathStringView> stdin_param;


  do {
    OptionalToken param_type_token = GetNextToken();
    if (!param_type_token) {
      return kParseDirectiveResultError;
    }
    if (stdext::holds_alternative<JSONTokenizer::EndObjectToken>(
            *param_type_token)) {
      if (!command_param || !inputs_param || !outputs_param) {
        SetError(kErrorDidNotFindAllExpectedKeys);
        return kParseDirectiveResultError;
      }
      ProduceSystemCommand(SystemCommandParams(
          *command_param, std::move(*inputs_param), std::move(*outputs_param),
          soft_outputs_param ? std::move(*soft_outputs_param)
                             : std::vector<ebb::lib::JSONPathStringView>(),
          deps_param, stdout_param, stderr_param, stdin_param));
      return kParseDirectiveResultSuccess;
    } else if (
        !stdext::holds_alternative<JSONTokenizer::JSONStringViewToken>(
             *param_type_token)) {
      SetError(kErrorUnexpectedToken);
      return kParseDirectiveResultError;
    }

    const ebb::lib::JSONStringView& param_type =
        stdext::get<JSONTokenizer::JSONStringViewToken>(*param_type_token)
            .string_view;
    if (param_type.IsEqual("cmd")) {
      if (command_param) {
        SetError(kErrorMultiplyDefinedKey);
        return kParseDirectiveResultError;
      }

      OptionalToken command_token =
          GetNextTokenType<JSONTokenizer::JSONStringViewToken>();
      if (!command_token) {
        return kParseDirectiveResultError;
      }
      command_param.emplace(
          stdext::get<JSONTokenizer::JSONStringViewToken>(*command_token)
              .string_view);
    } else if (param_type.IsEqual("in")) {
      if (inputs_param) {
        SetError(kErrorMultiplyDefinedKey);
        return kParseDirectiveResultError;
      }

      inputs_param = ParsePathList();
      if (!inputs_param) {
        return kParseDirectiveResultError;
      }
    } else if (param_type.IsEqual("out")) {
      if (outputs_param) {
        SetError(kErrorMultiplyDefinedKey);
        return kParseDirectiveResultError;
      }

      outputs_param = ParsePathList();
      if (!outputs_param) {
        return kParseDirectiveResultError;
      }
    } else if (param_type.IsEqual("soft_out")) {
      if (soft_outputs_param) {
        SetError(kErrorMultiplyDefinedKey);
        return kParseDirectiveResultError;
      }

      soft_outputs_param = ParsePathList();
      if (!soft_outputs_param) {
        return kParseDirectiveResultError;
      }
    } else if (param_type.IsEqual("deps")) {
      if (deps_param) {
        SetError(kErrorMultiplyDefinedKey);
        return kParseDirectiveResultError;
      }

      OptionalToken deps_token =
          GetNextTokenType<JSONTokenizer::JSONStringViewToken>();
      if (!deps_token) {
        return kParseDirectiveResultError;
      }
      deps_param.emplace(
          stdext::get<JSONTokenizer::JSONStringViewToken>(*deps_token)
              .string_view);
    } else if (param_type.IsEqual("stdout")) {
      if (stdout_param) {
        SetError(kErrorMultiplyDefinedKey);
        return kParseDirectiveResultError;
      }

      OptionalToken stdout_token =
          GetNextTokenType<JSONTokenizer::JSONStringViewToken>();
      if (!stdout_token) {
        return kParseDirectiveResultError;
      }
      stdout_param.emplace(
          stdext::get<JSONTokenizer::JSONStringViewToken>(*stdout_token)
              .string_view);
    } else if (param_type.IsEqual("stderr")) {
      if (stderr_param) {
        SetError(kErrorMultiplyDefinedKey);
        return kParseDirectiveResultError;
      }

      OptionalToken stderr_token =
          GetNextTokenType<JSONTokenizer::JSONStringViewToken>();
      if (!stderr_token) {
        return kParseDirectiveResultError;
      }
      stderr_param.emplace(
          stdext::get<JSONTokenizer::JSONStringViewToken>(*stderr_token)
              .string_view);
    } else if (param_type.IsEqual("stdin")) {
      if (stdin_param) {
        SetError(kErrorMultiplyDefinedKey);
        return kParseDirectiveResultError;
      }

      OptionalToken stdin_token =
          GetNextTokenType<JSONTokenizer::JSONStringViewToken>();
      if (!stdin_token) {
        return kParseDirectiveResultError;
      }
      stdin_param.emplace(
          stdext::get<JSONTokenizer::JSONStringViewToken>(*stdin_token)
              .string_view);
    }
  } while(true);
}

RegistryParser::ParseDirectiveResult RegistryParser::ParseBuildDirective() {
  OptionalToken token = GetNextToken();
  if (!token) {
    return kParseDirectiveResultError;
  }
  if (stdext::holds_alternative<JSONTokenizer::EndListToken>(*token)) {
    return kParseDirectiveResultListEnd;
  }
  if (!stdext::holds_alternative<JSONTokenizer::JSONStringViewToken>(
           *token)) {
    SetError(kErrorUnexpectedToken);
    return kParseDirectiveResultError;
  }

  ProduceBuildPath(
      stdext::get<JSONTokenizer::JSONStringViewToken>(*token).string_view);

  return kParseDirectiveResultSuccess;
}

stdext::optional<std::vector<ebb::lib::JSONPathStringView>>
RegistryParser::ParsePathList() {
  if (!GetNextTokenType<JSONTokenizer::StartListToken>()) {
    return stdext::nullopt;
  }

  std::vector<ebb::lib::JSONPathStringView> result;
  do {
    OptionalToken token = GetNextToken();
    if (!token) {
      return stdext::nullopt;
    }
    if (stdext::holds_alternative<JSONTokenizer::EndListToken>(*token)) {
      return stdext::optional<std::vector<ebb::lib::JSONPathStringView>>(
          std::move(result));
    } else if (
        stdext::holds_alternative<JSONTokenizer::JSONStringViewToken>(
            *token)) {
      result.emplace_back(
          stdext::get<JSONTokenizer::JSONStringViewToken>(*token)
              .string_view);
    } else {
      SetError(kErrorUnexpectedToken);
      return stdext::nullopt;
    }
  } while(true);
}

void RegistryParser::ProduceIncludePath(ebb::lib::JSONPathStringView path) {
  ebb::Push<ErrorOrDirective>(
      output_queue_, Directive(IncludeParams(path)));
}

void RegistryParser::ProduceSystemCommand(
    SystemCommandParams&& system_command_params) {
  ebb::Push<ErrorOrDirective>(
      output_queue_, Directive(std::move(system_command_params)));
}

void RegistryParser::ProduceBuildPath(ebb::lib::JSONPathStringView path) {
  ebb::Push<ErrorOrDirective>(
      output_queue_, Directive(BuildParams(path)));
}

}  // namespace respire
