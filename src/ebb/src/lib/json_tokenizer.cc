#include "lib/json_tokenizer.h"

#include <functional>

namespace ebb {
namespace lib {

JSONTokenizer::JSONTokenizer(
    Environment* env, BatchedQueue<ErrorOrUInts>* input_queue,
    BatchedQueue<ErrorOrTokens>* output_queue, bool persisted_input_memory)
    : persisted_input_memory_(persisted_input_memory),
      input_queue_(input_queue),
      push_batcher_(output_queue),
      consumer_(env, std::bind(&JSONTokenizer::Run, this)) {
  Push<>(consumer_.queue());
}

void JSONTokenizer::Run() {
  // Push the initial top-level scope.
  scope_stack_.push_back(kTopLevelWaitingForValue);

  do {
    Pull<ErrorOrUInts> input_pull(input_queue_->queue());

    if (stdext::holds_alternative<int>(*input_pull.data())) {
      if (stdext::get<int>(*input_pull.data()) == 0) {
        // We've hit the end of the stream, we're done now.
        if (scope_stack_.back() == kTopLevelWaitingForValue ||
            scope_stack_.back() == kTopLevelWaitingForComma) {
          // Okay, we're in a valid state to reach the end of the stream.
          // Success!
          push_batcher_.PushSignal(kSuccess);
        } else {
          push_batcher_.PushSignal(kErrorUnexpectedEndOfStream);
        }
      } else {
        // If there was an error reading the input, propagate it forward.
        push_batcher_.PushSignal(kErrorInputStream);
      }
      // Regardless of whether it was an error or success, we are done parsing.
      return;
    }

    // Process the new bytes we've received.
    const stdext::span<uint8_t>& input_span =
        stdext::get<stdext::span<uint8_t>>(*input_pull.data());

    bool quit = false;
    for (size_t i = 0; i < input_span.size(); ++i) {
      if (!ParseNextChar(
               reinterpret_cast<const char*>(input_span.data() + i))) {
        quit = true;
        break;
      }
    }

    if (string_value_start_) {
      // We can't track string pieces across buffers, so save what we have now
      // if we're done with this buffer.
      AddStringPiece(reinterpret_cast<const char*>(
          input_span.data()) + input_span.size());
    }

    if (quit) {
      break;
    }
  } while(true);

  // An error must have occurred, drain the rest of the input.
  // TODO: When we have functionality for cancelling, don't drain, just
  //       terminate.
  do {
    Pull<ErrorOrUInts> input_pull(input_queue_->queue());

    if (stdext::holds_alternative<int>(*input_pull.data())) {
      return;
    }
  } while(true);
}

bool JSONTokenizer::ParseNextChar(const char* c_ptr) {
  char c = *c_ptr;

  switch (scope_stack_.back()) {
    case kStringWaitingForChar:
      return ParseCharInString(c_ptr);  
    case kStringWaitingForEscapedChar:
      return ParseEscapedCharInString(c_ptr);
    default: {}
  }

  // Ignore any whitespace not inside of a string.
  if (IsWhitespace(c)) {
    return true;
  }

  switch (scope_stack_.back()) {
    case kObjectWaitingForKey:
      return ParseWaitingForKey(c);
    case kTopLevelWaitingForValue:
    case kListWaitingForValue:
    case kObjectWaitingForValue:
      return ParseWaitingForValue(c);
    case kTopLevelWaitingForComma:
    case kListWaitingForComma:
    case kObjectWaitingForComma:
      return ParseWaitingForComma(c);
    case kObjectWaitingForColon:
      return ParseWaitingForColon(c);
    default: {}
  }

  // It should be impossible to be in any other state.
  assert(false);
  return false;
}

bool JSONTokenizer::ParseWaitingForValue(char c) {
  // Get the state transition ready for when we're done parsing the next
  // value.
  switch(scope_stack_.back()) {
    case kListWaitingForValue: {
      scope_stack_.back() = kListWaitingForComma;
    } break;
    case kObjectWaitingForValue: {
      scope_stack_.back() = kObjectWaitingForComma;
    } break;
    case kTopLevelWaitingForValue: {
      scope_stack_.back() = kTopLevelWaitingForComma;
    } break;
    default: {
      assert(false);
    } break;
  }

  if (c == '"') {
    scope_stack_.push_back(kStringWaitingForChar);
  } else if (c == '[') {
    push_batcher_.PushData(StartListToken());
    scope_stack_.push_back(kListWaitingForValue);
  } else if (c == '{') {
    push_batcher_.PushData(StartObjectToken());
    scope_stack_.push_back(kObjectWaitingForKey);
  } else if (IsClosingScopeCharacter(c)) {
    return ParseClosingScopeCharacter(c);
  } else {
    push_batcher_.PushSignal(kErrorInvalidToken);
    return false;
  }
  return true;
}

bool JSONTokenizer::ParseWaitingForKey(char c) {
  if (c == '"') {
    scope_stack_.back() = kObjectWaitingForColon;
    scope_stack_.push_back(kStringWaitingForChar);
    return true;
  } else if (IsClosingScopeCharacter(c)) {
    return ParseClosingScopeCharacter(c);
  } else {
    push_batcher_.PushSignal(kErrorInvalidToken);
    return false;
  }
}

bool JSONTokenizer::ParseWaitingForComma(char c) {
  if (c == ',') {
    switch (scope_stack_.back()) {
      case kTopLevelWaitingForComma: {
        scope_stack_.back() = kTopLevelWaitingForValue;
      } break;
      case kListWaitingForComma: {
        scope_stack_.back() = kListWaitingForValue;
      } break;
      case kObjectWaitingForComma: {
        scope_stack_.back() = kObjectWaitingForKey;
      } break;
      default: {}
    }
    return true;
  } else if (IsClosingScopeCharacter(c)) {
    return ParseClosingScopeCharacter(c);
  } else {
    push_batcher_.PushSignal(kErrorInvalidToken);
    return false;
  }
}

bool JSONTokenizer::ParseWaitingForColon(char c) {
  if (c == ':') {
    scope_stack_.back() = kObjectWaitingForValue;
    return true;
  } else {
    push_batcher_.PushSignal(kErrorInvalidToken);
    return false;
  }
}


bool JSONTokenizer::IsClosingScopeCharacter(char c) {
  return c == '}' || c == ']';
}

bool JSONTokenizer::IsWhitespace(char c) {
  switch (c) {
    case ' ':
    case '\t':
    case '\n':
    case '\r':
      return true;
    default:
      return false;
  }
}

bool JSONTokenizer::ParseClosingScopeCharacter(char c) {
  ParseState closing_state = scope_stack_.back();
  scope_stack_.pop_back();

  if (c == ']') {
    if (closing_state != kListWaitingForValue &&
        closing_state != kListWaitingForComma) {
      push_batcher_.PushSignal(kErrorInvalidToken);
      return false;
    } else {
      push_batcher_.PushData(EndListToken());
      return true;
    }
  } else if (c == '}') {
    if (closing_state != kObjectWaitingForKey &&
        closing_state != kObjectWaitingForComma) {
      push_batcher_.PushSignal(kErrorInvalidToken);
      return false;
    } else {
      push_batcher_.PushData(EndObjectToken());
      return true;
    }
  } else {
    assert(false);
    return false;
  }
}

bool JSONTokenizer::ParseCharInString(const char* c_ptr) {
  if (persisted_input_memory_) {
    if (string_value_start_ == nullptr) {
      string_value_start_ = c_ptr;
    }
  }

  char c = *c_ptr;
  if (c == '"') {
    // If the string has ended, compile the last part of it into a string
    // and create a new string token.
    if (!string_value_start_) {
      string_value_start_ = c_ptr;
    }
    // Ensure that the current string is constructed and fully put together.
    AddStringPiece(c_ptr);

    // Add the token to the queue.
    if (persisted_input_memory_) {
      push_batcher_.PushData(JSONStringViewToken(
          JSONStringView(string_value_start_,
                         static_cast<size_t>(c_ptr - string_value_start_))));
      string_value_start_ = nullptr;
    } else {
      push_batcher_.PushData(StringToken(std::move(*current_string_)));
    }
    scope_stack_.pop_back();
  } else if (c == '\\') {
    if (string_value_start_) {
      AddStringPiece(c_ptr);
    }
    scope_stack_.back() = kStringWaitingForEscapedChar;
  } else if (string_value_start_ == nullptr) {
      string_value_start_ = c_ptr;
  }
  return true;
}

bool JSONTokenizer::ParseEscapedCharInString(const char* c_ptr) {
  char c = *c_ptr;
  if (c != '\\' && c != '"') {
    push_batcher_.PushSignal(kErrorInvalidToken);
    return false;
  }
  if (string_value_start_ == nullptr) {
    string_value_start_ = c_ptr;
  }
  scope_stack_.back() = kStringWaitingForChar;
  return true;
}

void JSONTokenizer::AddStringPiece(const char* end) {
  if (persisted_input_memory_) {
    // If the input memory is persistent, then we don't have to worry about
    // chopping up the input string, we can just continue to remember the start
    // as usual.
    return;
  }

  if (!current_string_.has_value()) {
    current_string_.emplace(string_value_start_, end);
  } else {
    current_string_->insert(current_string_->end(), string_value_start_, end);
  }
  string_value_start_ = nullptr;
}

}  // namespace lib
}  // namespace ebb
