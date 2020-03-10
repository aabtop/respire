#include "lib/json_tokenizer.h"

#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "ebbpp.h"

namespace ebb {
namespace lib {

namespace {
const int kDefaultThreadCount = 1;
const int kDefaultStackSize = 16 * 1024;
const int kDefaultInputBufferSize = 1000;
const int kDefaultOutputBufferSize = 1000;

void TestExpectedTokenSequence(
    const std::string& input_string,
    const std::vector<JSONTokenizer::Token>& expected_token_sequence) {
  Environment env(kDefaultThreadCount, kDefaultStackSize);

  BatchedQueueWithMemory<ErrorOrUInts, kDefaultInputBufferSize>
      input_queue(&env);
  BatchedQueueWithMemory<JSONTokenizer::ErrorOrTokens, kDefaultOutputBufferSize>
      output_queue(&env);

  JSONTokenizer tokenizer(&env, &input_queue, &output_queue, false);

  // Remove the constness...
  std::vector<char> input_string_copy(input_string.begin(), input_string.end());

  // Push the test input and then push success.
  Push<ErrorOrUInts>(
      input_queue.queue(), stdext::span<uint8_t>(
          reinterpret_cast<uint8_t*>(input_string_copy.data()),
          input_string_copy.size()));
  Push<ErrorOrUInts>(input_queue.queue(), 0);

  size_t cur_token = 0;
  do {
    Pull<JSONTokenizer::ErrorOrTokens> pull(output_queue.queue());
    JSONTokenizer::ErrorOrTokens& value = *pull.data();

    if (stdext::holds_alternative<JSONTokenizer::Error>(value)) {
      // Ensure that if we get a error value, it is a success message, and
      // also that we have processed all expected tokens.
      EXPECT_EQ(JSONTokenizer::kSuccess,
                stdext::get<JSONTokenizer::Error>(value));
      ASSERT_EQ(expected_token_sequence.size(), cur_token);
      break;
    } else {
      ASSERT_TRUE(stdext::holds_alternative<stdext::span<
                      JSONTokenizer::Token>>(value));
      auto& token_span =
          stdext::get<stdext::span<JSONTokenizer::Token>>(value);

      for (const auto& token : token_span) {
        // Ensure that we receive the correct output sequence of tokens.
        ASSERT_LT(cur_token, expected_token_sequence.size());
        EXPECT_EQ(expected_token_sequence[cur_token], token);
        ++cur_token;
      }
    }
  } while(true);  
}
}  // internal

TEST(JSONTokenizerTest, SimpleEmptyList) {
  TestExpectedTokenSequence(
    "[]", {
      JSONTokenizer::StartListToken(),
      JSONTokenizer::EndListToken()
    });
}

TEST(JSONTokenizerTest, SimpleEmptyObject) {
  TestExpectedTokenSequence(
    "{}", {
      JSONTokenizer::StartObjectToken(),
      JSONTokenizer::EndObjectToken()
    });
}

TEST(JSONTokenizerTest, EmptyInput) {
  TestExpectedTokenSequence("", {});
}

TEST(JSONTokenizerTest, SimpleString) {
  TestExpectedTokenSequence(
    "\"hello there!\"", {
      JSONTokenizer::StringToken("hello there!"),
    });
}

TEST(JSONTokenizerTest, SimpleEscapedString) {
  TestExpectedTokenSequence(
    "\"hello \\\"there\\\"!\"", {
      JSONTokenizer::StringToken("hello \"there\"!"),
    });
}

TEST(JSONTokenizerTest, SimpleNestedList) {
  TestExpectedTokenSequence(
    "[[]]", {
      JSONTokenizer::StartListToken(),
      JSONTokenizer::StartListToken(),
      JSONTokenizer::EndListToken(),
      JSONTokenizer::EndListToken()
    });
}

TEST(JSONTokenizerTest, ManyNestedLists) {
  TestExpectedTokenSequence(
    "[[[[[[[[[[]]]]]]]]]]", {
      JSONTokenizer::StartListToken(),
      JSONTokenizer::StartListToken(),
      JSONTokenizer::StartListToken(),
      JSONTokenizer::StartListToken(),
      JSONTokenizer::StartListToken(),
      JSONTokenizer::StartListToken(),
      JSONTokenizer::StartListToken(),
      JSONTokenizer::StartListToken(),
      JSONTokenizer::StartListToken(),
      JSONTokenizer::StartListToken(),
      JSONTokenizer::EndListToken(),
      JSONTokenizer::EndListToken(),
      JSONTokenizer::EndListToken(),
      JSONTokenizer::EndListToken(),
      JSONTokenizer::EndListToken(),
      JSONTokenizer::EndListToken(),
      JSONTokenizer::EndListToken(),
      JSONTokenizer::EndListToken(),
      JSONTokenizer::EndListToken(),
      JSONTokenizer::EndListToken()
    });
}

TEST(JSONTokenizerTest, ManyNestedObjects) {
  TestExpectedTokenSequence(
    "{\"key\":{\"key\":{\"key\":{\"key\":{\"key\":{}}}}}}", {
      JSONTokenizer::StartObjectToken(),
      JSONTokenizer::StringToken("key"),
      JSONTokenizer::StartObjectToken(),
      JSONTokenizer::StringToken("key"),
      JSONTokenizer::StartObjectToken(),
      JSONTokenizer::StringToken("key"),
      JSONTokenizer::StartObjectToken(),
      JSONTokenizer::StringToken("key"),
      JSONTokenizer::StartObjectToken(),
      JSONTokenizer::StringToken("key"),
      JSONTokenizer::StartObjectToken(),
      JSONTokenizer::EndObjectToken(),
      JSONTokenizer::EndObjectToken(),
      JSONTokenizer::EndObjectToken(),
      JSONTokenizer::EndObjectToken(),
      JSONTokenizer::EndObjectToken(),
      JSONTokenizer::EndObjectToken()
    });
}

TEST(JSONTokenizerTest, SimpleNestedListWithWhitespace) {
  TestExpectedTokenSequence(
    "  [\t   [\n\n]\r\n]\n\r ", {
      JSONTokenizer::StartListToken(),
      JSONTokenizer::StartListToken(),
      JSONTokenizer::EndListToken(),
      JSONTokenizer::EndListToken()
    });
}

TEST(JSONTokenizerTest, SingleEntryObject) {
  TestExpectedTokenSequence(
    "{                         "
    "  \"foo\": \"bar\"        "
    "}                         ", {
      JSONTokenizer::StartObjectToken(),
      JSONTokenizer::StringToken("foo"),
      JSONTokenizer::StringToken("bar"),
      JSONTokenizer::EndObjectToken()
    });
}

TEST(JSONTokenizerTest, MultiEntryObject) {
  TestExpectedTokenSequence(
    "{                         "
    "  \"foo\": \"bar\",       "
    "  \"cat\": \"soup\",       "
    "  \"teddy\": \"bear\",       "
    "}                         ", {
      JSONTokenizer::StartObjectToken(),
      JSONTokenizer::StringToken("foo"),
      JSONTokenizer::StringToken("bar"),
      JSONTokenizer::StringToken("cat"),
      JSONTokenizer::StringToken("soup"),
      JSONTokenizer::StringToken("teddy"),
      JSONTokenizer::StringToken("bear"),
      JSONTokenizer::EndObjectToken()
    });
}

TEST(JSONTokenizerTest, MultiItemList) {
  TestExpectedTokenSequence(
    "[                                   "
    "  \"foo\", \"bar\", \"bear\"        "
    "]                                   ", {
      JSONTokenizer::StartListToken(),
      JSONTokenizer::StringToken("foo"),
      JSONTokenizer::StringToken("bar"),
      JSONTokenizer::StringToken("bear"),
      JSONTokenizer::EndListToken()
    });
}

TEST(JSONTokenizerTest, ListsObjectsAndStringsTest) {
  TestExpectedTokenSequence(
    "[                                   "
    "  {                                 "
    "    \"hello\" : \"how are you\",    "
    "    \"bats\" : [                    "
    "      \"buttercups\",               "
    "      \"spices\",                   "
    "    ],                              "
    "  },                                "
    "  {                                 "
    "    \"final\": \"object\",          "
    "  },                                "
    "],                                  ", {
      JSONTokenizer::StartListToken(),
      JSONTokenizer::StartObjectToken(),
      JSONTokenizer::StringToken("hello"),
      JSONTokenizer::StringToken("how are you"),
      JSONTokenizer::StringToken("bats"),
      JSONTokenizer::StartListToken(),
      JSONTokenizer::StringToken("buttercups"),
      JSONTokenizer::StringToken("spices"),
      JSONTokenizer::EndListToken(),
      JSONTokenizer::EndObjectToken(),
      JSONTokenizer::StartObjectToken(),
      JSONTokenizer::StringToken("final"),
      JSONTokenizer::StringToken("object"),
      JSONTokenizer::EndObjectToken(),
      JSONTokenizer::EndListToken()
    });
}

}  // namespace lib
}  // namespace ebb
