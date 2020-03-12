#include <gtest/gtest.h>

#include <stdio.h>

#include <fstream>
#include <functional>

#include "build_targets.h"
#include "stdext/file_system.h"
#include "test_system_command.h"

using stdext::file_system::Path;
using stdext::file_system::PathStrRef;
using stdext::file_system::TemporaryDirectory;

namespace respire {

namespace {
std::string EscapeForJSON(std::string in) {
  std::string out;
  out.reserve(in.size());

  for (size_t i = 0; i < in.size(); ++i) {
    switch (in[i]) {
      case '\\': {
        out.push_back('\\');
        out.push_back('\\');        
      } break;
      case '"': {
        out.push_back('\\');
        out.push_back('"');
      } break;
      default: {
        out.push_back(in[i]);
      }
    }
  }
  return out;
}

std::string EscapeForJSON(const Path& path) {
  return EscapeForJSON(path.str());
}

void WriteToFile(PathStrRef path, const std::string& contents) {
  std::ofstream out(path.c_str());
  out << contents;
  WaitForFilesystemTimeResolution();
}

respire::OptionalError BuildTargetRegistry(
    respire::Environment* env, const std::string& contents,
    const std::vector<Path>& target_list) {
  TemporaryDirectory temp_dir;

  Path registry_file(Join(temp_dir.path(), PathStrRef("initial.respire")));
  WriteToFile(registry_file, contents);

  // Create a "driver" respire file that will include the main content above,
  // and then go ahead and declare a build directive for each of the targets.
  Path targets_file(Join(temp_dir.path(), PathStrRef("targets.respire")));
  std::ostringstream oss;
  oss << "[{"
      << "\"inc\": [\"" << EscapeForJSON(registry_file) << "\"]},"
      << "{\"build\": [";

  for (const auto& target : target_list) {
    oss << "\"" << EscapeForJSON(target) << "\",";
  }
  oss << "]}]";
  WriteToFile(targets_file, oss.str());

  // Finally, build the driver include.
  return BuildTargets(env, targets_file);
}

void ExpectFileHasContents(const std::string& contents, PathStrRef path) {
  std::ifstream in(path.c_str());
  ASSERT_TRUE(in);
  std::stringstream buffer;
  buffer << in.rdbuf();
  EXPECT_EQ(contents, buffer.str()); 
}

using CommandList = std::vector<std::pair<std::string, int>>;

class SystemCommandRecorder {
 public:
  SystemCommandRecorder() {}

  CommandList TakeCommands() {
    CommandList ret;
    commands_.swap(ret);
    return std::move(ret);
  }

  Environment::SystemCommandFunction GetRecorderFunction() {
    return Environment::SystemCommandFunction(
        std::bind(&SystemCommandRecorder::AcceptCommand, this,
                   std::placeholders::_1, std::placeholders::_2,
                   std::placeholders::_3, std::placeholders::_4));
  }

 private:
  int AcceptCommand(
      const char* command,
      const stdext::optional<stdext::file_system::Path>& stdout_file,
      const stdext::optional<stdext::file_system::Path>& stderr_file,
      const stdext::optional<stdext::file_system::Path>& stdin_file) {
    std::pair<std::string, int> results(
        command,
        TestSystemCommand(command, stdout_file, stderr_file, stdin_file));
    commands_.push_back(results);
    return results.second;
  }

  CommandList commands_;
};

// Returns true if |before| appears once before any instance of |after| in the
// command list, and both |before| and |after| are present in the list.
// Returns false otherwise.
bool CommandAppearsOnceBefore(
    const CommandList& command_list,
    const std::string& before, const std::string& after) {
  bool found_before = false;

  for (const auto& command : command_list) {
    if (command.first == after) {
      if (found_before) {
        return true;
      } else {
        return false;
      }
    }

    if (command.first == before) {
      if (found_before) {
        // This function expects only *one* instance of the |before| command
        // before the |after| command.
        return false;
      } else {
        found_before = true;
      }
    }
  }

  return false;
}

bool AllCommandsSucceeded(const CommandList& command_list) {
  for (const auto& command : command_list) {
    if (command.second != 0) {
      return false;
    }
  }

  return true;
}

void RemoveFile(PathStrRef path) {
  int remove_result = remove(path.c_str());
  ASSERT_EQ(0, remove_result);
}

}  // namespace

TEST(BuildTargetsTest, SimpleSingleSystemCommand) {
  TemporaryDirectory temp_dir;
  Path out_file = Join(temp_dir.path(), PathStrRef("out.txt"));

  respire::Environment::Options options;
  options.system_command_function = &TestSystemCommand;
  respire::Environment env(options);
  respire::OptionalError maybe_error(BuildTargetRegistry(
      &env,
      "[{\"sc\": [{"
      "\"cmd\": \"echo " + EscapeForJSON(out_file) + ",a\","
      "\"in\": [],"
      "\"out\": [\"" + EscapeForJSON(out_file) + "\"],"
      "}]}]",
      {out_file}));
  EXPECT_FALSE(maybe_error.has_value());

  ExpectFileHasContents("a", out_file);
}

TEST(BuildTargetsTest, SingleSystemCommandAvoidsUnnecessaryRebuild) {
  TemporaryDirectory temp_dir;
  Path out_file = Join(temp_dir.path(), PathStrRef("out.txt"));

  SystemCommandRecorder command_recorder;
  respire::Environment::Options options;
  options.system_command_function = command_recorder.GetRecorderFunction();
  respire::Environment env(options);

  std::string command("echo " + out_file.str() + ",a");
  std::string registry_contents =
      "[{\"sc\": [{"
      "\"cmd\": \"" + EscapeForJSON(command) + "\","
      "\"in\": [],"
      "\"out\": [\"" + EscapeForJSON(out_file) + "\"],"
      "}]}]";

  respire::OptionalError maybe_error(BuildTargetRegistry(
      &env, registry_contents, {out_file}));
  EXPECT_FALSE(maybe_error.has_value());

  ExpectFileHasContents("a", out_file);


  CommandList command_list(command_recorder.TakeCommands());
  EXPECT_EQ(1, command_list.size());
  EXPECT_EQ(command, command_list[0].first);
  EXPECT_TRUE(AllCommandsSucceeded(command_list));

  maybe_error = BuildTargetRegistry(&env, registry_contents, {out_file});
  EXPECT_FALSE(maybe_error.has_value());

  command_list = command_recorder.TakeCommands();
  EXPECT_EQ(0, command_list.size());

  ExpectFileHasContents("a", out_file);
}

TEST(BuildTargetsTest, TwoNodeCommandChain) {
  TemporaryDirectory temp_dir;
  Path out_file1 = Join(temp_dir.path(), PathStrRef("out1.txt"));
  Path out_file2 = Join(temp_dir.path(), PathStrRef("out2.txt"));

  SystemCommandRecorder command_recorder;
  respire::Environment::Options options;
  options.system_command_function = command_recorder.GetRecorderFunction();
  respire::Environment env(options);

  std::string command1 = "echo " + EscapeForJSON(out_file1) + ",a";
  std::string command2 = "cat " + EscapeForJSON(out_file2) + "," +
      EscapeForJSON(out_file1) + "," + EscapeForJSON(out_file1);

  std::string registry_contents =
      "["
      "{\"sc\": [{"
      "\"cmd\": \"" + EscapeForJSON(command1) + "\","
      "\"in\": [],"
      "\"out\": [\"" + EscapeForJSON(out_file1) + "\"],"
      "}]},"
      "{\"sc\": [{"
      "\"cmd\":"
          "\"" + EscapeForJSON(command2) + "\","
      "\"in\": [\"" + EscapeForJSON(out_file1) + "\"],"
      "\"out\": [\"" + EscapeForJSON(out_file2) + "\"],"
      "}]}"      
      "]";

  respire::OptionalError maybe_error(BuildTargetRegistry(
      &env, registry_contents, {out_file2}));
  EXPECT_FALSE(maybe_error.has_value());

  ExpectFileHasContents("a", out_file1);
  ExpectFileHasContents("aa", out_file2);

  CommandList command_list(command_recorder.TakeCommands());
  EXPECT_EQ(2, command_list.size());
  EXPECT_TRUE(AllCommandsSucceeded(command_list));
  EXPECT_TRUE(CommandAppearsOnceBefore(command_list, command1, command2));

  maybe_error = BuildTargetRegistry(&env, registry_contents, {out_file2});
  EXPECT_FALSE(maybe_error.has_value());
  command_list = command_recorder.TakeCommands();
  EXPECT_EQ(0, command_list.size());

  // Check that if the root of the chain is deleted, the entire chain is
  // re-computed.
  RemoveFile(out_file1);
  maybe_error = BuildTargetRegistry(&env, registry_contents, {out_file2});
  EXPECT_FALSE(maybe_error.has_value());
  command_list = command_recorder.TakeCommands();
  EXPECT_EQ(2, command_list.size());
  EXPECT_TRUE(AllCommandsSucceeded(command_list));
  EXPECT_TRUE(CommandAppearsOnceBefore(command_list, command1, command2));

  // Check that if the last item in the chain is modified, then only that
  // item is re-computed.
  RemoveFile(out_file2);
  maybe_error = BuildTargetRegistry(&env, registry_contents, {out_file2});
  EXPECT_FALSE(maybe_error.has_value());
  command_list = command_recorder.TakeCommands();
  ASSERT_EQ(1, command_list.size());
  EXPECT_TRUE(AllCommandsSucceeded(command_list));
  EXPECT_EQ(command2, command_list[0].first);
}

TEST(BuildTargetsTest, FileModificationCausesRebuild) {
  TemporaryDirectory temp_dir;
  Path static_file1 = Join(temp_dir.path(), PathStrRef("static1.txt"));
  Path out_file1 = Join(temp_dir.path(), PathStrRef("out1.txt"));
  Path out_file2 = Join(temp_dir.path(), PathStrRef("out2.txt"));

  SystemCommandRecorder command_recorder;
  respire::Environment::Options options;
  options.system_command_function = command_recorder.GetRecorderFunction();
  respire::Environment env(options);

  std::string command1 = "cat " + EscapeForJSON(out_file1) + "," +
      EscapeForJSON(static_file1) + "," + EscapeForJSON(static_file1);
  std::string command2 = "cat " + EscapeForJSON(out_file2) + "," +
      EscapeForJSON(out_file1) + "," + EscapeForJSON(out_file1);

  WriteToFile(static_file1, "foo");

  std::string registry_contents =
      "["
      "{\"sc\": [{"
      "\"cmd\":"
          "\"" + EscapeForJSON(command1) + "\","
      "\"in\": [\"" + EscapeForJSON(static_file1) + "\"],"
      "\"out\": [\"" + EscapeForJSON(out_file1) + "\"],"
      "}]},"
      "{\"sc\": [{"
      "\"cmd\":"
          "\"" + EscapeForJSON(command2) + "\","
      "\"in\": [\"" + EscapeForJSON(out_file1) + "\"],"
      "\"out\": [\"" + EscapeForJSON(out_file2) + "\"],"
      "}]}"
      "]";

  respire::OptionalError maybe_error(BuildTargetRegistry(
      &env, registry_contents, {out_file2}));
  EXPECT_FALSE(maybe_error.has_value());

  ExpectFileHasContents("foofoo", out_file1);
  ExpectFileHasContents("foofoofoofoo", out_file2);

  CommandList command_list(command_recorder.TakeCommands());
  EXPECT_EQ(2, command_list.size());
  EXPECT_TRUE(AllCommandsSucceeded(command_list));
  EXPECT_TRUE(CommandAppearsOnceBefore(command_list, command1, command2));

  maybe_error = BuildTargetRegistry(&env, registry_contents, {out_file2});
  EXPECT_FALSE(maybe_error.has_value());
  command_list = command_recorder.TakeCommands();
  EXPECT_EQ(0, command_list.size());

  // Modify the file and re-build.
  WriteToFile(static_file1, "bleh");

  maybe_error = BuildTargetRegistry(&env, registry_contents, {out_file2});
  EXPECT_FALSE(maybe_error.has_value());
  command_list = command_recorder.TakeCommands();
  EXPECT_EQ(2, command_list.size());
  EXPECT_TRUE(AllCommandsSucceeded(command_list));
  EXPECT_TRUE(CommandAppearsOnceBefore(command_list, command1, command2));

  ExpectFileHasContents("blehbleh", out_file1);
  ExpectFileHasContents("blehblehblehbleh", out_file2);
}

TEST(BuildTargetsTest, UnmodifiedSoftOutputIsOkay) {
  TemporaryDirectory temp_dir;
  Path static_file1 = Join(temp_dir.path(), PathStrRef("static1.txt"));
  Path timestamp_file1 = Join(temp_dir.path(), PathStrRef("timestamp1.txt"));
  Path soft_out_file1 = Join(temp_dir.path(), PathStrRef("soft_out1.txt"));
  Path out_file2 = Join(temp_dir.path(), PathStrRef("out2.txt"));

  SystemCommandRecorder command_recorder;
  respire::Environment::Options options;
  options.system_command_function = command_recorder.GetRecorderFunction();
  respire::Environment env(options);

  std::string command1 = "copy_file_if_different " +
      EscapeForJSON(static_file1) + "," +
      EscapeForJSON(soft_out_file1) + "," + EscapeForJSON(timestamp_file1);
  std::string command2 = "cat " + EscapeForJSON(out_file2) + "," +
      EscapeForJSON(soft_out_file1) + "," + EscapeForJSON(soft_out_file1);

  WriteToFile(static_file1, "foo");

  std::string registry_contents =
      "["
      "{\"sc\": [{"
      "\"cmd\":"
          "\"" + EscapeForJSON(command1) + "\","
      "\"in\": [\"" + EscapeForJSON(static_file1) + "\"],"
      "\"out\": [\"" + EscapeForJSON(timestamp_file1) + "\"],"
      "\"soft_out\": [\"" + EscapeForJSON(soft_out_file1) + "\"],"
      "}]},"
      "{\"sc\": [{"
      "\"cmd\":"
          "\"" + EscapeForJSON(command2) + "\","
      "\"in\": [\"" + EscapeForJSON(soft_out_file1) + "\"],"
      "\"out\": [\"" + EscapeForJSON(out_file2) + "\"],"
      "}]}"
      "]";

  respire::OptionalError maybe_error(BuildTargetRegistry(
      &env, registry_contents, {out_file2}));
  EXPECT_FALSE(maybe_error.has_value());

  ExpectFileHasContents("foo", soft_out_file1);
  ExpectFileHasContents("foofoo", out_file2);

  CommandList command_list(command_recorder.TakeCommands());
  EXPECT_EQ(2, command_list.size());
  EXPECT_TRUE(AllCommandsSucceeded(command_list));
  EXPECT_TRUE(CommandAppearsOnceBefore(command_list, command1, command2));

  maybe_error = BuildTargetRegistry(&env, registry_contents, {out_file2});
  EXPECT_FALSE(maybe_error.has_value());
  command_list = command_recorder.TakeCommands();
  EXPECT_EQ(0, command_list.size());

  // Modify the file and re-build.
  WriteToFile(static_file1, "bleh");

  maybe_error = BuildTargetRegistry(&env, registry_contents, {out_file2});
  EXPECT_FALSE(maybe_error.has_value());
  command_list = command_recorder.TakeCommands();
  EXPECT_EQ(2, command_list.size());
  EXPECT_TRUE(AllCommandsSucceeded(command_list));
  EXPECT_TRUE(CommandAppearsOnceBefore(command_list, command1, command2));

  ExpectFileHasContents("bleh", soft_out_file1);
  ExpectFileHasContents("blehbleh", out_file2);

  maybe_error = BuildTargetRegistry(&env, registry_contents, {out_file2});
  EXPECT_FALSE(maybe_error.has_value());
  command_list = command_recorder.TakeCommands();
  EXPECT_EQ(0, command_list.size());

  // Touch the file, but don't change its contents, which should result in the
  // second command not needing to be executed since the soft output was not
  // modified.
  WriteToFile(static_file1, "bleh");

  maybe_error = BuildTargetRegistry(&env, registry_contents, {out_file2});
  EXPECT_FALSE(maybe_error.has_value());
  command_list = command_recorder.TakeCommands();
  EXPECT_EQ(1, command_list.size());
  EXPECT_EQ(command1, command_list[0].first);
  EXPECT_TRUE(AllCommandsSucceeded(command_list));

  ExpectFileHasContents("bleh", soft_out_file1);
  ExpectFileHasContents("blehbleh", out_file2);  
}

TEST(BuildTargetsTest, MultipleInputsCommandNode) {
  TemporaryDirectory temp_dir;
  Path out_file1 = Join(temp_dir.path(), PathStrRef("out1.txt"));
  Path out_file2 = Join(temp_dir.path(), PathStrRef("out2.txt"));
  Path out_file3 = Join(temp_dir.path(), PathStrRef("out3.txt"));
  Path out_file4 = Join(temp_dir.path(), PathStrRef("out4.txt"));

  SystemCommandRecorder command_recorder;
  respire::Environment::Options options;
  options.system_command_function = command_recorder.GetRecorderFunction();
  respire::Environment env(options);

  std::string command1 = "echo " + EscapeForJSON(out_file1) + ",a";
  std::string command2 = "echo " + EscapeForJSON(out_file2) + ",b";
  std::string command3 = "echo " + EscapeForJSON(out_file3) + ",c";
  std::string command4 = "cat " + EscapeForJSON(out_file4) + "," +
      EscapeForJSON(out_file1) + "," + EscapeForJSON(out_file2) +
      "," + EscapeForJSON(out_file3);

  std::string registry_contents =
      "["
      "{\"sc\": [{"
      "\"cmd\": \"" + EscapeForJSON(command1) + "\","
      "\"in\": [],"
      "\"out\": [\"" + EscapeForJSON(out_file1) + "\"],"
      "}]},"
      "{\"sc\": [{"
      "\"cmd\": \"" + EscapeForJSON(command2) + "\","
      "\"in\": [],"
      "\"out\": [\"" + EscapeForJSON(out_file2) + "\"],"
      "}]},"
      "{\"sc\": [{"
      "\"cmd\": \"" + EscapeForJSON(command3) + "\","
      "\"in\": [],"
      "\"out\": [\"" + EscapeForJSON(out_file3) + "\"],"
      "}]},"
      "{\"sc\": [{"
      "\"cmd\":"
          "\"" + EscapeForJSON(command4) + "\","
      "\"in\": ["
      "\"" + EscapeForJSON(out_file1) + "\","
      "\"" + EscapeForJSON(out_file2) + "\","
      "\"" + EscapeForJSON(out_file3) + "\","
      "],"
      "\"out\": [\"" + EscapeForJSON(out_file4) + "\"],"
      "}]}"      
      "]";

  respire::OptionalError maybe_error(BuildTargetRegistry(
      &env, registry_contents, {out_file4}));
  EXPECT_FALSE(maybe_error.has_value());

  ExpectFileHasContents("a", out_file1);
  ExpectFileHasContents("b", out_file2);
  ExpectFileHasContents("c", out_file3);
  ExpectFileHasContents("abc", out_file4);

  CommandList command_list(command_recorder.TakeCommands());
  EXPECT_EQ(4, command_list.size());
  EXPECT_TRUE(AllCommandsSucceeded(command_list));
  EXPECT_TRUE(CommandAppearsOnceBefore(command_list, command1, command4));
  EXPECT_TRUE(CommandAppearsOnceBefore(command_list, command2, command4));
  EXPECT_TRUE(CommandAppearsOnceBefore(command_list, command3, command4));

  maybe_error = BuildTargetRegistry(&env, registry_contents, {out_file4});
  EXPECT_FALSE(maybe_error.has_value());
  command_list = command_recorder.TakeCommands();
  EXPECT_EQ(0, command_list.size());

  // Check that if a dependency is deleted, the dependency and the parent are
  // recomputed.
  RemoveFile(out_file1);
  maybe_error = BuildTargetRegistry(&env, registry_contents, {out_file4});
  EXPECT_FALSE(maybe_error.has_value());
  command_list = command_recorder.TakeCommands();
  EXPECT_EQ(2, command_list.size());
  EXPECT_TRUE(AllCommandsSucceeded(command_list));
  EXPECT_TRUE(CommandAppearsOnceBefore(command_list, command1, command4));

  // Check that if the final element is deleted, only that element is
  // recomputed.
  RemoveFile(out_file4);
  maybe_error = BuildTargetRegistry(&env, registry_contents, {out_file4});
  EXPECT_FALSE(maybe_error.has_value());
  command_list = command_recorder.TakeCommands();
  ASSERT_EQ(1, command_list.size());
  EXPECT_TRUE(AllCommandsSucceeded(command_list));
  EXPECT_EQ(command4, command_list[0].first);
}

TEST(BuildTargetsTest, SimpleRegistryNode) {
  TemporaryDirectory temp_dir;
  Path include_file = Join(temp_dir.path(), PathStrRef("include1.respire"));
  Path out_file1 = Join(temp_dir.path(), PathStrRef("out1.txt"));

  SystemCommandRecorder command_recorder;
  respire::Environment::Options options;
  options.system_command_function = command_recorder.GetRecorderFunction();
  respire::Environment env(options);

  std::string command = "echo " + EscapeForJSON(out_file1) + ",a";

  std::string registry_contents1 =
      "["
      "{"
      "\"inc\": [\"" + EscapeForJSON(include_file) + "\"],"
      "}"
      "]";

  std::string registry_contents2 =
      "[{\"sc\": [{"
      "\"cmd\": \"" + EscapeForJSON(command) + "\","
      "\"in\": [],"
      "\"out\": [\"" + EscapeForJSON(out_file1) + "\"],"
      "}]}]";

  WriteToFile(include_file, registry_contents2);

  respire::OptionalError maybe_error(BuildTargetRegistry(
      &env, registry_contents1, {out_file1}));
  EXPECT_FALSE(maybe_error.has_value());

  ExpectFileHasContents("a", out_file1);

  CommandList command_list(command_recorder.TakeCommands());
  ASSERT_EQ(1, command_list.size());
  EXPECT_TRUE(AllCommandsSucceeded(command_list));
  EXPECT_EQ(command, command_list[0].first);
}

TEST(BuildTargetsTest, RegistryNodeDependency) {
  TemporaryDirectory temp_dir;
  Path include_file = Join(temp_dir.path(), PathStrRef("include1.respire"));
  Path out_file1 = Join(temp_dir.path(), PathStrRef("out1.txt"));
  Path out_file2 = Join(temp_dir.path(), PathStrRef("out2.txt"));

  SystemCommandRecorder command_recorder;
  respire::Environment::Options options;
  options.system_command_function = command_recorder.GetRecorderFunction();
  respire::Environment env(options);

  std::string command1 = "echo " + EscapeForJSON(out_file1) + ",a";
  std::string command2 = "cat " + EscapeForJSON(out_file2) + "," +
      EscapeForJSON(out_file1) + "," + EscapeForJSON(out_file1);

  std::string registry_contents1 =
      "["
      "{"
      "\"inc\": [\"" + EscapeForJSON(include_file) + "\"],"
      "},"
      "{\"sc\": [{"
      "\"cmd\":"
          "\"" + EscapeForJSON(command2) + "\","
      "\"in\": [\"" + EscapeForJSON(out_file1) + "\"],"
      "\"out\": [\"" + EscapeForJSON(out_file2) + "\"],"
      "}]}"      
      "]";

  std::string registry_contents2 =
      "[{\"sc\": [{"
      "\"cmd\": \"" + EscapeForJSON(command1) + "\","
      "\"in\": [],"
      "\"out\": [\"" + EscapeForJSON(out_file1) + "\"],"
      "}]}]";

  WriteToFile(include_file, registry_contents2);

  respire::OptionalError maybe_error(BuildTargetRegistry(
      &env, registry_contents1, {out_file2}));
  EXPECT_FALSE(maybe_error.has_value());

  ExpectFileHasContents("a", out_file1);
  ExpectFileHasContents("aa", out_file2);

  CommandList command_list(command_recorder.TakeCommands());
  EXPECT_EQ(2, command_list.size());
  EXPECT_TRUE(AllCommandsSucceeded(command_list));
  EXPECT_TRUE(CommandAppearsOnceBefore(command_list, command1, command2));

  maybe_error = BuildTargetRegistry(&env, registry_contents1, {out_file2});
  EXPECT_FALSE(maybe_error.has_value());
  command_list = command_recorder.TakeCommands();
  EXPECT_EQ(0, command_list.size());
}

TEST(BuildTargetsTest, RegistryNodeGenerated) {
  TemporaryDirectory temp_dir;
  Path include_file = Join(temp_dir.path(), PathStrRef("include1.respire"));
  Path out_file1 = Join(temp_dir.path(), PathStrRef("out1.txt"));

  SystemCommandRecorder command_recorder;
  respire::Environment::Options options;
  options.system_command_function = command_recorder.GetRecorderFunction();
  respire::Environment env(options);

  std::string command = "echo " + EscapeForJSON(out_file1) + ",a";

  std::string registry_contents2 =
      "[{\"sc\": [{"
      "\"cmd\": \"" + EscapeForJSON(command) + "\","
      "\"in\": [],"
      "\"out\": [\"" + EscapeForJSON(out_file1) + "\"],"
      "}]}]";

  std::string make_registry_command = "echo " + include_file.str() +
      ",," + registry_contents2;

  std::string registry_contents1 =
      "["
      "{\"sc\": [{"
      "\"cmd\":"
          "\"" + EscapeForJSON(make_registry_command) + "\","
      "\"in\": [],"
      "\"out\": [\"" + EscapeForJSON(include_file) + "\"],"
      "}]},"      
      "{"
      "\"inc\": [\"" + EscapeForJSON(include_file) + "\"],"
      "},"
      "]";

  respire::OptionalError maybe_error(BuildTargetRegistry(
      &env, registry_contents1, {out_file1}));
  EXPECT_FALSE(maybe_error.has_value());

  ExpectFileHasContents(registry_contents2, include_file);
  ExpectFileHasContents("a", out_file1);

  CommandList command_list(command_recorder.TakeCommands());
  EXPECT_EQ(2, command_list.size());
  EXPECT_TRUE(AllCommandsSucceeded(command_list));
  EXPECT_TRUE(CommandAppearsOnceBefore(
      command_list, make_registry_command, command));

  RemoveFile(include_file);;
  maybe_error = BuildTargetRegistry(&env, registry_contents1, {out_file1});
  EXPECT_FALSE(maybe_error.has_value());
  command_list = command_recorder.TakeCommands();
  ASSERT_EQ(1, command_list.size());
  EXPECT_TRUE(AllCommandsSucceeded(command_list));
  EXPECT_EQ(make_registry_command, command_list[0].first);
}

TEST(BuildTargetsTest, DepsFileModificationsTriggerRebuild) {
  TemporaryDirectory temp_dir;
  Path static_file1 = Join(temp_dir.path(), PathStrRef("static1.txt"));
  Path static_file2 = Join(temp_dir.path(), PathStrRef("static2.txt"));
  Path deps_file = Join(temp_dir.path(), PathStrRef("deps.txt"));
  Path out_file1 = Join(temp_dir.path(), PathStrRef("out1.txt"));

  SystemCommandRecorder command_recorder;
  respire::Environment::Options options;
  options.system_command_function = command_recorder.GetRecorderFunction();
  respire::Environment env(options);

  std::string command = "cat_file_list " + EscapeForJSON(out_file1) + "," +
      EscapeForJSON(deps_file);

  WriteToFile(static_file1, "foo");
  WriteToFile(static_file2, "bar");
  {
    std::ofstream out(deps_file.c_str());
    out << static_file1.c_str() << std::endl;
    out << static_file2.c_str() << std::endl;
  }

  std::string registry_contents =
      "["
      "{\"sc\": [{"
      "\"cmd\":"
          "\"" + EscapeForJSON(command) + "\","
      "\"in\": [],"
      "\"out\": [\"" + EscapeForJSON(out_file1) + "\"],"
      "\"deps\": \"" + EscapeForJSON(deps_file) + "\","
      "}]},"
      "]";

  respire::OptionalError maybe_error(BuildTargetRegistry(
      &env, registry_contents, {out_file1}));
  EXPECT_FALSE(maybe_error.has_value());

  ExpectFileHasContents("foobar", out_file1);

  CommandList command_list(command_recorder.TakeCommands());
  EXPECT_EQ(1, command_list.size());
  EXPECT_TRUE(AllCommandsSucceeded(command_list));
  EXPECT_EQ(command, command_list[0].first);

  maybe_error = BuildTargetRegistry(&env, registry_contents, {out_file1});
  EXPECT_FALSE(maybe_error.has_value());
  command_list = command_recorder.TakeCommands();
  EXPECT_EQ(0, command_list.size());

  // Modify the file and re-build.
  WriteToFile(static_file1, "bleh");

  maybe_error = BuildTargetRegistry(&env, registry_contents, {out_file1});
  EXPECT_FALSE(maybe_error.has_value());
  command_list = command_recorder.TakeCommands();
  ASSERT_EQ(1, command_list.size());
  EXPECT_TRUE(AllCommandsSucceeded(command_list));
  EXPECT_EQ(command, command_list[0].first);

  ExpectFileHasContents("blehbar", out_file1);

  WriteToFile(static_file2, "grark");

  maybe_error = BuildTargetRegistry(&env, registry_contents, {out_file1});
  EXPECT_FALSE(maybe_error.has_value());
  command_list = command_recorder.TakeCommands();
  EXPECT_EQ(1, command_list.size());
  EXPECT_TRUE(AllCommandsSucceeded(command_list));
  EXPECT_EQ(command, command_list[0].first);

  ExpectFileHasContents("blehgrark", out_file1);
}

TEST(BuildTargetsTest, StdRedirectTest) {
  TemporaryDirectory temp_dir;
  Path stdin_file = Join(temp_dir.path(), PathStrRef("stdin.txt"));
  Path stdout_file = Join(temp_dir.path(), PathStrRef("stdout.txt"));
  Path stderr_file = Join(temp_dir.path(), PathStrRef("stderr.txt"));
  Path final_file = Join(temp_dir.path(), PathStrRef("final.txt"));

  SystemCommandRecorder command_recorder;
  respire::Environment::Options options;
  options.system_command_function = command_recorder.GetRecorderFunction();
  respire::Environment env(options);

  std::string create_stdout_file_command =
      "echo stdout,o";
  std::string create_stderr_file_command =
      "echo stderr,e";
  std::string create_stdin_file_command =
      "echo " + EscapeForJSON(stdin_file) + ",i";
  std::string cat_all_files_together_command =
      "cat " + EscapeForJSON(final_file) + "," + EscapeForJSON(stdout_file) +
      "," + EscapeForJSON(stderr_file) + ",stdin";

  std::string registry_contents =
      "["
      "{\"sc\": [{"
      "\"cmd\": \"" + EscapeForJSON(create_stdout_file_command) + "\","
      "\"in\": [],"
      "\"out\": [],"
      "\"stdout\": \"" + EscapeForJSON(stdout_file) + "\","
      "}]},"
      "{\"sc\": [{"
      "\"cmd\": \"" + EscapeForJSON(create_stderr_file_command) + "\","
      "\"in\": [],"
      "\"out\": [],"
      "\"stderr\": \"" + EscapeForJSON(stderr_file) + "\","
      "}]},"
      "{\"sc\": [{"
      "\"cmd\": \"" + EscapeForJSON(create_stdin_file_command) + "\","
      "\"in\": [],"
      "\"out\": [\"" + EscapeForJSON(stdin_file) + "\"],"
      "}]},"
      "{\"sc\": [{"
      "\"cmd\": \"" + EscapeForJSON(cat_all_files_together_command) + "\","
      "\"in\": [\"" + EscapeForJSON(stdout_file) + "\", \"" +
                      EscapeForJSON(stderr_file) + "\"],"
      "\"out\": [\"" + EscapeForJSON(final_file) + "\"],"
      "\"stdin\": \"" + EscapeForJSON(stdin_file) + "\","
      "}]},"
      "]";

  respire::OptionalError maybe_error(BuildTargetRegistry(
      &env, registry_contents, {final_file}));
  if (maybe_error.has_value()) {
    std::cout << "Error: " << maybe_error->str() << std::endl;
  }
  EXPECT_FALSE(maybe_error.has_value());

  ExpectFileHasContents("o", stdout_file);
  ExpectFileHasContents("e", stderr_file);
  ExpectFileHasContents("i", stdin_file);
  ExpectFileHasContents("oei", final_file);

  CommandList command_list(command_recorder.TakeCommands());
  EXPECT_EQ(4, command_list.size());
  EXPECT_TRUE(AllCommandsSucceeded(command_list));
  EXPECT_TRUE(CommandAppearsOnceBefore(
      command_list, create_stdin_file_command, cat_all_files_together_command));
  EXPECT_TRUE(CommandAppearsOnceBefore(
      command_list, create_stdout_file_command, cat_all_files_together_command));
  EXPECT_TRUE(CommandAppearsOnceBefore(
      command_list, create_stderr_file_command, cat_all_files_together_command));

  maybe_error = BuildTargetRegistry(
      &env, registry_contents, {final_file});
  EXPECT_FALSE(maybe_error.has_value());
  command_list = command_recorder.TakeCommands();
  EXPECT_EQ(0, command_list.size());
}

}  // namespace respire
