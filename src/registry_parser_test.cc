#include "registry_parser.h"

#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "ebbpp.h"

using ebb::lib::JSONTokenizer;

namespace respire {

const int kDefaultThreadCount = 1;
const int kDefaultStackSize = 16 * 1024;
const int kDefaultInputBufferSize = 1000;
const int kDefaultOutputBufferSize = 1000;

void TestExpectedTokenSequence(
    const std::vector<JSONTokenizer::Token>& input_tokens,
    const std::vector<RegistryParser::Directive>& expected_directive_sequence) {
  ebb::Environment env(kDefaultThreadCount, kDefaultStackSize);

  ebb::QueueWithMemory<RegistryParser::ErrorOrDirective,
                       kDefaultOutputBufferSize> output_queue(&env);
  ebb::lib::BatchedQueueWithMemory<JSONTokenizer::ErrorOrTokens,
                                   kDefaultInputBufferSize> input_queue(&env);

  RegistryParser registry_parser(&env, &input_queue, &output_queue);

  // Push the input data.
  {
    ebb::lib::PushBatcher<JSONTokenizer::ErrorOrTokens>
        push_batcher(&input_queue);

    for (const auto& token : input_tokens) {
      push_batcher.PushData(token);
    }

    push_batcher.PushSignal(JSONTokenizer::kSuccess);
  }

  // Pull out and verify the output data.
  size_t cur_directive = 0;
  do {
    ebb::Pull<RegistryParser::ErrorOrDirective> pull(&output_queue);
    RegistryParser::ErrorOrDirective& value = *pull.data();

    if (stdext::holds_alternative<RegistryParser::Error>(value)) {
      // Ensure that if we get a error value, it is a success message, and
      // also that we have processed all expected directives.
      EXPECT_EQ(RegistryParser::kErrorSuccess,
                stdext::get<RegistryParser::Error>(value));
      ASSERT_EQ(expected_directive_sequence.size(), cur_directive);
      break;
    } else {
      ASSERT_TRUE(stdext::holds_alternative<RegistryParser::Directive>(value));
      auto& directive = stdext::get<RegistryParser::Directive>(value);
      
      // Ensure that we receive the correct output sequence of directivess.
      ASSERT_LT(cur_directive, expected_directive_sequence.size());
      EXPECT_EQ(expected_directive_sequence[cur_directive], directive);
      ++cur_directive;
    }
  } while(true);
}


TEST(RegistryParserTest, SimpleEmptyList) {
  TestExpectedTokenSequence(
    {
      JSONTokenizer::StartListToken(),
      JSONTokenizer::EndListToken()
    },
    {});
}

namespace {
ebb::lib::JSONStringView MakeStringView(const char* str) {
  return ebb::lib::JSONStringView(str, strlen(str));
}

ebb::lib::JSONPathStringView MakePathStringView(const char* str) {
  return ebb::lib::JSONPathStringView(MakeStringView(str));
}

JSONTokenizer::JSONStringViewToken MakeStringViewToken(const char* str) {
  return JSONTokenizer::JSONStringViewToken(MakeStringView(str));
}
JSONTokenizer::JSONStringViewToken MakeStringViewToken(const std::string& str) {
  return JSONTokenizer::JSONStringViewToken(MakeStringView(str.c_str()));
}
}  // namespace

TEST(RegistryParserTest, SingleIncludeEntry) {
  const char* kTestIncludePath = "my/test/path";

  TestExpectedTokenSequence(
    {
      JSONTokenizer::StartListToken(),
      JSONTokenizer::StartObjectToken(),
      MakeStringViewToken("inc"),
      JSONTokenizer::StartListToken(),
      MakeStringViewToken(kTestIncludePath),
      JSONTokenizer::EndListToken(),
      JSONTokenizer::EndObjectToken(),
      JSONTokenizer::EndListToken()
    },
    {
      RegistryParser::IncludeParams(MakeStringView(kTestIncludePath))
    });
}

TEST(RegistryParserTest, MultipleIncludeEntries) {
  const char* kTestIncludePath1 = "my/test/path/1";
  const char* kTestIncludePath2 = "my/test/path/2";

  TestExpectedTokenSequence(
    {
      JSONTokenizer::StartListToken(),
      JSONTokenizer::StartObjectToken(),
      MakeStringViewToken("inc"),
      JSONTokenizer::StartListToken(),
      MakeStringViewToken(kTestIncludePath1),
      MakeStringViewToken(kTestIncludePath2),
      JSONTokenizer::EndListToken(),
      JSONTokenizer::EndObjectToken(),
      JSONTokenizer::EndListToken()
    },
    {
      RegistryParser::IncludeParams(MakeStringView(kTestIncludePath1)),
      RegistryParser::IncludeParams(MakeStringView(kTestIncludePath2))
    });
}

TEST(RegistryParserTest, BuildEntry) {
  const char* kTestBuildPath = "my/test/path";

  TestExpectedTokenSequence(
    {
      JSONTokenizer::StartListToken(),
      JSONTokenizer::StartObjectToken(),
      MakeStringViewToken("build"),
      JSONTokenizer::StartListToken(),
      MakeStringViewToken(kTestBuildPath),
      JSONTokenizer::EndListToken(),
      JSONTokenizer::EndObjectToken(),
      JSONTokenizer::EndListToken()
    },
    {
      RegistryParser::BuildParams(MakeStringView(kTestBuildPath)),
    });
}

TEST(RegistryParserTest, MultipleBuildEntries) {
  const char* kTestBuildPath1 = "my/test/path/1";
  const char* kTestBuildPath2 = "my/test/path/2";

  TestExpectedTokenSequence(
    {
      JSONTokenizer::StartListToken(),
      JSONTokenizer::StartObjectToken(),
      MakeStringViewToken("build"),
      JSONTokenizer::StartListToken(),
      MakeStringViewToken(kTestBuildPath1),
      MakeStringViewToken(kTestBuildPath2),
      JSONTokenizer::EndListToken(),
      JSONTokenizer::EndObjectToken(),
      JSONTokenizer::EndListToken()
    },
    {
      RegistryParser::BuildParams(MakeStringView(kTestBuildPath1)),
      RegistryParser::BuildParams(MakeStringView(kTestBuildPath2))
    });
}

TEST(RegistryParserTest, MultipleIncludeEntriesAsSeparateObjects) {
  const char* kTestIncludePath1 = "my/test/path/1";
  const char* kTestIncludePath2 = "my/test/path/2";

  TestExpectedTokenSequence(
    {
      JSONTokenizer::StartListToken(),

      JSONTokenizer::StartObjectToken(),
      MakeStringViewToken("inc"),
      JSONTokenizer::StartListToken(),
      MakeStringViewToken(kTestIncludePath1),
      JSONTokenizer::EndListToken(),
      JSONTokenizer::EndObjectToken(),

      JSONTokenizer::StartObjectToken(),
      MakeStringViewToken("inc"),
      JSONTokenizer::StartListToken(),
      MakeStringViewToken(kTestIncludePath2),
      JSONTokenizer::EndListToken(),
      JSONTokenizer::EndObjectToken(),

      JSONTokenizer::EndListToken()
    },
    {
      RegistryParser::IncludeParams(MakeStringView(kTestIncludePath1)),
      RegistryParser::IncludeParams(MakeStringView(kTestIncludePath2))
    });
}

TEST(RegistryParserTest, SingleSystemCommandEntry) {
  const char* kTestCommandLine = "test command line";
  const char* kTestInputPath1 = "test/input/path/1";
  const char* kTestInputPath2 = "test/input/path/2";
  const char* kTestOutputPath = "test/output/path";

  TestExpectedTokenSequence(
    {
      JSONTokenizer::StartListToken(),
      JSONTokenizer::StartObjectToken(),
      MakeStringViewToken("sc"),
      JSONTokenizer::StartListToken(),
      JSONTokenizer::StartObjectToken(),
      MakeStringViewToken("cmd"),
      MakeStringViewToken(kTestCommandLine),
      MakeStringViewToken("in"),
      JSONTokenizer::StartListToken(),
      MakeStringViewToken(kTestInputPath1),
      MakeStringViewToken(kTestInputPath2),
      JSONTokenizer::EndListToken(),
      MakeStringViewToken("out"),
      JSONTokenizer::StartListToken(),
      MakeStringViewToken(kTestOutputPath),
      JSONTokenizer::EndListToken(),
      JSONTokenizer::EndObjectToken(),
      JSONTokenizer::EndListToken(),
      JSONTokenizer::EndObjectToken(),
      JSONTokenizer::EndListToken()
    },
    {
      RegistryParser::SystemCommandParams(
          MakeStringView(kTestCommandLine),
          {MakeStringView(kTestInputPath1), MakeStringView(kTestInputPath2)},
          {MakeStringView(kTestOutputPath)})
    });
}

TEST(RegistryParserTest, SingleSystemCommandEntryWithDepsFile) {
  const char* kTestCommandLine = "test command line";
  const char* kTestInputPath1 = "test/input/path/1";
  const char* kTestInputPath2 = "test/input/path/2";
  const char* kTestOutputPath = "test/output/path";
  const char* kTestDepsFilePath = "test/depsfile/path";

  TestExpectedTokenSequence(
    {
      JSONTokenizer::StartListToken(),
      JSONTokenizer::StartObjectToken(),
      MakeStringViewToken("sc"),
      JSONTokenizer::StartListToken(),
      JSONTokenizer::StartObjectToken(),
      MakeStringViewToken("cmd"),
      MakeStringViewToken(kTestCommandLine),
      MakeStringViewToken("in"),
      JSONTokenizer::StartListToken(),
      MakeStringViewToken(kTestInputPath1),
      MakeStringViewToken(kTestInputPath2),
      JSONTokenizer::EndListToken(),
      MakeStringViewToken("out"),
      JSONTokenizer::StartListToken(),
      MakeStringViewToken(kTestOutputPath),
      JSONTokenizer::EndListToken(),
      MakeStringViewToken("deps"),
      MakeStringViewToken(kTestDepsFilePath),
      JSONTokenizer::EndObjectToken(),
      JSONTokenizer::EndListToken(),
      JSONTokenizer::EndObjectToken(),
      JSONTokenizer::EndListToken()
    },
    {
      RegistryParser::SystemCommandParams(
          MakeStringView(kTestCommandLine),
          {MakeStringView(kTestInputPath1), MakeStringView(kTestInputPath2)},
          {MakeStringView(kTestOutputPath)},
          {}, MakePathStringView(kTestDepsFilePath))
    });
}

TEST(RegistryParserTest, SystemCommandEntryWithSoftOutputs) {
  const char* kTestCommandLine = "test command line";
  const char* kTestInputPath = "test/input/path";
  const char* kTestOutputPath = "test/output/path";
  const char* kTestSoftOutputPath1 = "test/softoutput/path/1";
  const char* kTestSoftOutputPath2 = "test/softoutput/path/2";

  TestExpectedTokenSequence(
    {
      JSONTokenizer::StartListToken(),
      JSONTokenizer::StartObjectToken(),
      MakeStringViewToken("sc"),
      JSONTokenizer::StartListToken(),
      JSONTokenizer::StartObjectToken(),
      MakeStringViewToken("cmd"),
      MakeStringViewToken(kTestCommandLine),
      MakeStringViewToken("in"),
      JSONTokenizer::StartListToken(),
      MakeStringViewToken(kTestInputPath),
      JSONTokenizer::EndListToken(),
      MakeStringViewToken("out"),
      JSONTokenizer::StartListToken(),
      MakeStringViewToken(kTestOutputPath),
      JSONTokenizer::EndListToken(),
      MakeStringViewToken("soft_out"),
      JSONTokenizer::StartListToken(),
      MakeStringViewToken(kTestSoftOutputPath1),
      MakeStringViewToken(kTestSoftOutputPath2),
      JSONTokenizer::EndListToken(),
      JSONTokenizer::EndObjectToken(),
      JSONTokenizer::EndListToken(),
      JSONTokenizer::EndObjectToken(),
      JSONTokenizer::EndListToken()
    },
    {
      RegistryParser::SystemCommandParams(
          MakeStringView(kTestCommandLine),
          {MakeStringView(kTestInputPath)},
          {MakeStringView(kTestOutputPath)},
          {MakeStringView(kTestSoftOutputPath1),
           MakeStringView(kTestSoftOutputPath2)})
    });
}

TEST(RegistryParserTest, MultipleSystemCommandEntry) {
  const RegistryParser::SystemCommandParams kSysCommandParams1(
      MakeStringView("test command line 1"),
      {MakeStringView("test/input/path/1/1"),
       MakeStringView("test/input/path/1/2")},
      {MakeStringView("test/output/path/1")});
  const RegistryParser::SystemCommandParams kSysCommandParams2(
      MakeStringView("test command line 2"),
      {MakeStringView("test/input/path/2/1"),
       MakeStringView("test/input/path/2/2")},
      {MakeStringView("test/output/path/2")});

  TestExpectedTokenSequence(
    {
      JSONTokenizer::StartListToken(),
      JSONTokenizer::StartObjectToken(),
      MakeStringViewToken("sc"),
      JSONTokenizer::StartListToken(),

      JSONTokenizer::StartObjectToken(),
      MakeStringViewToken("cmd"),
      JSONTokenizer::JSONStringViewToken(kSysCommandParams1.command),
      MakeStringViewToken("in"),
      JSONTokenizer::StartListToken(),
      JSONTokenizer::JSONStringViewToken(kSysCommandParams1.inputs[0]),
      JSONTokenizer::JSONStringViewToken(kSysCommandParams1.inputs[1]),
      JSONTokenizer::EndListToken(),
      MakeStringViewToken("out"),
      JSONTokenizer::StartListToken(),
      JSONTokenizer::JSONStringViewToken(kSysCommandParams1.outputs[0]),
      JSONTokenizer::EndListToken(),
      JSONTokenizer::EndObjectToken(),

      JSONTokenizer::StartObjectToken(),
      MakeStringViewToken("cmd"),
      JSONTokenizer::JSONStringViewToken(kSysCommandParams2.command),
      MakeStringViewToken("in"),
      JSONTokenizer::StartListToken(),
      JSONTokenizer::JSONStringViewToken(kSysCommandParams2.inputs[0]),
      JSONTokenizer::JSONStringViewToken(kSysCommandParams2.inputs[1]),
      JSONTokenizer::EndListToken(),
      MakeStringViewToken("out"),
      JSONTokenizer::StartListToken(),
      JSONTokenizer::JSONStringViewToken(kSysCommandParams2.outputs[0]),
      JSONTokenizer::EndListToken(),
      JSONTokenizer::EndObjectToken(),
      JSONTokenizer::EndListToken(),
      JSONTokenizer::EndObjectToken(),
      JSONTokenizer::EndListToken()
    },
    {
      kSysCommandParams1,
      kSysCommandParams2
    });
}

TEST(RegistryParserTest, IncludeAndSystemCommandEntries) {
  const char* kTestIncludePath = "my/test/path";

  const char* kTestCommandLine = "test command line";
  const char* kTestInputPath1 = "test/input/path/1";
  const char* kTestInputPath2 = "test/input/path/2";
  const char* kTestOutputPath = "test/output/path";

  TestExpectedTokenSequence(
    {
      JSONTokenizer::StartListToken(),

      // Include entry
      JSONTokenizer::StartObjectToken(),
      MakeStringViewToken("inc"),
      JSONTokenizer::StartListToken(),
      MakeStringViewToken(kTestIncludePath),
      JSONTokenizer::EndListToken(),
      JSONTokenizer::EndObjectToken(),

      // System command entry
      JSONTokenizer::StartObjectToken(),
      MakeStringViewToken("sc"),
      JSONTokenizer::StartListToken(),
      JSONTokenizer::StartObjectToken(),
      MakeStringViewToken("cmd"),
      MakeStringViewToken(kTestCommandLine),
      MakeStringViewToken("in"),
      JSONTokenizer::StartListToken(),
      MakeStringViewToken(kTestInputPath1),
      MakeStringViewToken(kTestInputPath2),
      JSONTokenizer::EndListToken(),
      MakeStringViewToken("out"),
      JSONTokenizer::StartListToken(),
      MakeStringViewToken(kTestOutputPath),
      JSONTokenizer::EndListToken(),
      JSONTokenizer::EndObjectToken(),
      JSONTokenizer::EndListToken(),
      JSONTokenizer::EndObjectToken(),

      JSONTokenizer::EndListToken()
    },
    {
      RegistryParser::IncludeParams(MakeStringView(kTestIncludePath)),
      RegistryParser::SystemCommandParams(
          MakeStringView(kTestCommandLine),
          {MakeStringView(kTestInputPath1), MakeStringView(kTestInputPath2)},
          {MakeStringView(kTestOutputPath)})
    });
}

TEST(RegistryParserTest, SimpleStdRedirectSystemCommandEntry) {
  const char* kTestCommandLine = "test command line";
  const char* kTestInputPath = "test/input/path/1";
  const char* kTestOutputPath = "test/output/path";
  const char* kTestStdOutPath = "test/output/stdout";
  const char* kTestStdErrPath = "test/output/stderr";
  const char* kTestStdInPath = "test/input/stdin";

  TestExpectedTokenSequence(
    {
      JSONTokenizer::StartListToken(),
      JSONTokenizer::StartObjectToken(),
      MakeStringViewToken("sc"),
      JSONTokenizer::StartListToken(),
      JSONTokenizer::StartObjectToken(),
      MakeStringViewToken("cmd"),
      MakeStringViewToken(kTestCommandLine),
      MakeStringViewToken("in"),
      JSONTokenizer::StartListToken(),
      MakeStringViewToken(kTestInputPath),
      JSONTokenizer::EndListToken(),
      MakeStringViewToken("out"),
      JSONTokenizer::StartListToken(),
      MakeStringViewToken(kTestOutputPath),
      JSONTokenizer::EndListToken(),
      MakeStringViewToken("stdout"),
      MakeStringViewToken(kTestStdOutPath),
      MakeStringViewToken("stderr"),
      MakeStringViewToken(kTestStdErrPath),
      MakeStringViewToken("stdin"),
      MakeStringViewToken(kTestStdInPath),
      JSONTokenizer::EndObjectToken(),
      JSONTokenizer::EndListToken(),
      JSONTokenizer::EndObjectToken(),
      JSONTokenizer::EndListToken()
    },
    {
      RegistryParser::SystemCommandParams(
          MakeStringView(kTestCommandLine), {MakeStringView(kTestInputPath)},
          {MakeStringView(kTestOutputPath)}, {}, stdext::nullopt,
          MakePathStringView(kTestStdOutPath),
          MakePathStringView(kTestStdErrPath),
          MakePathStringView(kTestStdInPath))
    });
}

}  // namespace respire
