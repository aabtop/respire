#ifndef __EBB_LIB_JSON_TOKENIZER_H__
#define __EBB_LIB_JSON_TOKENIZER_H__

#include "ebbpp.h"

#include "lib/batching.h"
#include "lib/json_string_view.h"
#include "lib/types.h"
#include "stdext/fixed_vector.h"
#include "stdext/span.h"
#include "stdext/variant.h"

#include <string>
#include <vector>

namespace ebb {
namespace lib {

// Reads from the input stream until it reads a Error signal.
class JSONTokenizer {
 public:
  enum Error {
    kSuccess,
    kErrorInputStream,
    kErrorInvalidToken,
    kErrorUnexpectedEndOfStream,
  };

  struct StartListToken {
    bool operator==(const StartListToken& rhs) const { return true; }
  };
  struct EndListToken {
    bool operator==(const EndListToken& rhs) const { return true; }
  };
  struct StartObjectToken {
    bool operator==(const StartObjectToken& rhs) const { return true; }
  };
  struct EndObjectToken {
    bool operator==(const EndObjectToken& rhs) const { return true; }
  };
  struct StringToken {
    StringToken(const std::string& str) : str(str) {}
    StringToken(std::string&& str) : str(std::move(str)) {}
    bool operator==(const StringToken& rhs) const { return str == rhs.str; }

    std::string str;
  };
  struct JSONStringViewToken {
    JSONStringViewToken(JSONStringView string_view)
        : string_view(string_view) {}
    bool operator==(const JSONStringViewToken& rhs) const {
      return string_view == rhs.string_view;
    }

    JSONStringView string_view;
  };

  using Token = stdext::variant<
                  StartListToken,
                  EndListToken,
                  StartObjectToken,
                  EndObjectToken,
                  StringToken,
                  JSONStringViewToken>;

  using ErrorOrTokens = Batch<Error, Token>;

  JSONTokenizer(Environment* env, BatchedQueue<ErrorOrUInts>* input_queue,
                BatchedQueue<ErrorOrTokens>* output_queue,
                bool persisted_input_memory);

 private:
  enum ParseState {
    kTopLevelWaitingForValue,
    kTopLevelWaitingForComma,
    kListWaitingForValue,
    kListWaitingForComma,
    kObjectWaitingForKey,
    kObjectWaitingForColon,
    kObjectWaitingForValue,
    kObjectWaitingForComma,
    kStringWaitingForChar,
    kStringWaitingForEscapedChar,
  };

  void Run();
  bool ParseNextChar(const char* c_ptr);
  bool ParseWaitingForValue(char c);
  bool IsWhitespace(char c);
  bool IsClosingScopeCharacter(char c);
  bool ParseClosingScopeCharacter(char c);
  bool ParseCharInString(const char* c_ptr);
  bool ParseEscapedCharInString(const char* c_ptr);
  bool ParseWaitingForKey(char c);
  bool ParseWaitingForComma(char c);
  bool ParseWaitingForColon(char c);
  void AddStringPiece(const char* end);

  // If true, it will be assumed that the input memory persists and so the
  // produced string tokens will be JSONStringViewTokens with references to the
  // original input data strings, instead of StringTokens with owned
  // std::strings.
  const bool persisted_input_memory_;

  BatchedQueue<ErrorOrUInts>* input_queue_;

  // Wrapper around the output queue to help with batching pushes together.
  PushBatcher<ErrorOrTokens> push_batcher_;

  // The stack of which list/object scope we are currently parsing within.
  std::vector<ParseState> scope_stack_;

  // The current string being built up across different input buffers.
  stdext::optional<std::string> current_string_;

  // Track the beginning of any string value we started reading.
  const char* string_value_start_ = nullptr;

  // Internal queue used to kick off the parsing process.
  ConsumerWithQueue<void, 1> consumer_;
};

}  // namespace lib
}  // namespace ebb

#endif  // __EBB_LIB_JSON_TOKENIZER_H__
