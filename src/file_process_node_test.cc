#include <fstream>
#include <gtest/gtest.h>
#include <sstream>

#include "environment.h"
#include "file_process_node.h"
#include "stdext/file_system.h"

using stdext::optional;
using stdext::file_system::GetLastModificationTime;
using stdext::file_system::Join;
using stdext::file_system::Path;
using stdext::file_system::PathStrRef;
using stdext::file_system::TemporaryDirectory;
using respire::Error;

namespace {
void WriteToFile(const PathStrRef& path, const std::string& contents) {
  std::ofstream out(path.c_str());
  ASSERT_TRUE(out);
  out << contents;
}

void ConcatenateToFile(
    const PathStrRef& input_file_1, const PathStrRef& input_file_2,
    const PathStrRef& output_file) {
  std::ofstream out(output_file.c_str());
  ASSERT_TRUE(out);
  std::ifstream in1(input_file_1.c_str());
  ASSERT_TRUE(in1);
  std::ifstream in2(input_file_2.c_str());
  ASSERT_TRUE(in2);
  out << in1.rdbuf() << in2.rdbuf();
}

std::string ReadFileContents(const PathStrRef& path) {
  std::ifstream in(path.c_str());
  std::stringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}
}  // namespace

TEST(FileProcessNodeTest, CanCreateFile) {
  respire::Environment env;

  TemporaryDirectory temp_dir;
  Path temp_file = Join(temp_dir.path(), PathStrRef("foo.txt"));
  std::string json_temp_file = ebb::lib::ToJSON(temp_file.str());

  const char* kFileContents = "bar";
  respire::FileProcessNode create_file_node(
      &env, {}, {ebb::lib::JSONPathStringView(json_temp_file)}, {},
      [&]() {
        WriteToFile(temp_file, kFileContents);
        return optional<Error>();
      });

  auto before_create = std::chrono::system_clock::now();
  respire::FileInfoNode::FuturePtr test_file(create_file_node.GetFileInfo());

  ASSERT_NE(nullptr, test_file->GetValue()->value());
  const respire::FileOutput::Value& value = *test_file->GetValue()->value();
  ASSERT_EQ(1, value.size());
  EXPECT_EQ(temp_file, value[0].filename.AsString());
  if (platform::kFileModificationTimeConsistentWithSystemClock) {
    EXPECT_GE(*value[0].last_modified_time, before_create);
  }

  EXPECT_EQ(kFileContents, ReadFileContents(temp_file));
}

TEST(FileProcessNodeTest, CanReturnError) {
  respire::Environment env;

  TemporaryDirectory temp_dir;
  Path temp_file = Join(temp_dir.path(), PathStrRef("foo.txt"));
  std::string json_temp_file = ebb::lib::ToJSON(temp_file.str());

  const char* kFileContents = "bar";
  respire::FileProcessNode create_file_node(
      &env, {}, {ebb::lib::JSONPathStringView(json_temp_file)}, {},
      [&]() { return Error("Made up test error!"); });

  respire::FileInfoNode::FuturePtr test_file(create_file_node.GetFileInfo());

  ASSERT_NE(nullptr, test_file->GetValue()->error());
}

TEST(FileProcessNodeTest, CanCreateMultipleFiles) {
  respire::Environment env;

  TemporaryDirectory temp_dir;
  Path temp_file1 = Join(temp_dir.path(), PathStrRef("foo1.txt"));
  std::string json_temp_file1 = ebb::lib::ToJSON(temp_file1.str());
  Path temp_file2 = Join(temp_dir.path(), PathStrRef("foo2.txt"));
  std::string json_temp_file2 = ebb::lib::ToJSON(temp_file2.str());

  const char* kFileContents1 = "bar1";
  const char* kFileContents2 = "bar2";
  respire::FileProcessNode create_file_node(
      &env, {}, {ebb::lib::JSONPathStringView(json_temp_file1),
                 ebb::lib::JSONPathStringView(json_temp_file2)}, {},
      [&]() {
        WriteToFile(temp_file1, kFileContents1);
        WriteToFile(temp_file2, kFileContents2);
        return optional<Error>();
      });

  auto before_create = std::chrono::system_clock::now();
  respire::FileInfoNode::FuturePtr test_files(create_file_node.GetFileInfo());

  ASSERT_NE(nullptr, test_files->GetValue()->value());
  const respire::FileOutput::Value& value = *test_files->GetValue()->value();
  ASSERT_EQ(2, value.size());
  EXPECT_EQ(temp_file1, value[0].filename.AsString());
  EXPECT_EQ(temp_file2, value[1].filename.AsString());
  if (platform::kFileModificationTimeConsistentWithSystemClock) {
    EXPECT_GE(*value[0].last_modified_time, before_create);
    EXPECT_GE(*value[1].last_modified_time, before_create);
  }

  EXPECT_EQ(kFileContents1, ReadFileContents(temp_file1));
  EXPECT_EQ(kFileContents2, ReadFileContents(temp_file2));
}

TEST(FileProcessNodeTest, WillNotReWriteToFileAfterItExists) {
  respire::Environment env;

  TemporaryDirectory temp_dir;
  Path temp_file = Join(temp_dir.path(), PathStrRef("foo.txt"));
  std::string json_temp_file = ebb::lib::ToJSON(temp_file.str());

  const char* kFileContents = "bar";
  respire::FileProcessNode create_file_node(
      &env, {}, {ebb::lib::JSONPathStringView(json_temp_file)}, {},
      [&]() {
        WriteToFile(temp_file, kFileContents);
        return optional<Error>();
      });

  auto before_create = std::chrono::system_clock::now();

  respire::FileInfoNode::FuturePtr test_file_first(
      create_file_node.GetFileInfo());
  ASSERT_NE(nullptr, test_file_first->GetValue()->value());
  const respire::FileOutput::Value& first_value =
      *test_file_first->GetValue()->value();
  if (platform::kFileModificationTimeConsistentWithSystemClock) {
    EXPECT_GE(*first_value[0].last_modified_time, before_create);
  }
  auto last_modified_time = GetLastModificationTime(temp_file);
  ASSERT_TRUE(last_modified_time.has_value());
  EXPECT_EQ(*first_value[0].last_modified_time, *last_modified_time);

  // Make sure that the second request does not result in an updated
  // modification time.
  respire::FileInfoNode::FuturePtr test_file_second(
      create_file_node.GetFileInfo());
  ASSERT_NE(nullptr, test_file_second->GetValue()->value());
  const respire::FileOutput::Value& second_value =
      *test_file_second->GetValue()->value();
  EXPECT_EQ(*first_value[0].last_modified_time,
            *second_value[0].last_modified_time);
}

TEST(FileProcessNodeTest, LackOfSoftOuputWillNotCauseRebuild) {
  respire::Environment env;

  TemporaryDirectory temp_dir;
  Path temp_out_file = Join(temp_dir.path(), PathStrRef("foo.txt"));
  std::string json_temp_out_file = ebb::lib::ToJSON(temp_out_file.str());
  Path temp_soft_out_file = Join(temp_dir.path(), PathStrRef("soft_foo.txt"));
  std::string json_temp_soft_out_file =
      ebb::lib::ToJSON(temp_soft_out_file.str());


  const char* kFileContents = "bar";
  respire::FileProcessNode create_file_node(
      &env, {}, {ebb::lib::JSONPathStringView(json_temp_out_file)},
                {ebb::lib::JSONPathStringView(json_temp_soft_out_file)},
      [&]() {
        WriteToFile(temp_out_file, kFileContents);
        return optional<Error>();
      });

  auto before_create = std::chrono::system_clock::now();

  respire::FileInfoNode::FuturePtr test_file_first(
      create_file_node.GetFileInfo());
  ASSERT_NE(nullptr, test_file_first->GetValue()->value());
  const respire::FileOutput::Value& first_value =
      *test_file_first->GetValue()->value();
  if (platform::kFileModificationTimeConsistentWithSystemClock) {
    EXPECT_GE(*first_value[0].last_modified_time, before_create);
  }
  auto last_modified_time = GetLastModificationTime(temp_out_file);
  ASSERT_TRUE(last_modified_time.has_value());
  EXPECT_EQ(*first_value[0].last_modified_time, *last_modified_time);

  // Make sure that the second request does not result in an updated
  // modification time.
  respire::FileInfoNode::FuturePtr test_file_second(
      create_file_node.GetFileInfo());
  ASSERT_NE(nullptr, test_file_second->GetValue()->value());
  const respire::FileOutput::Value& second_value =
      *test_file_second->GetValue()->value();
  EXPECT_EQ(*first_value[0].last_modified_time,
            *second_value[0].last_modified_time);
}

TEST(FileProcessNodeTest, ErrorWhenOutputFileUnmodified) {
  respire::Environment env;

  TemporaryDirectory temp_dir;
  Path temp_file = Join(temp_dir.path(), PathStrRef("foo.txt"));
  std::string json_temp_file = ebb::lib::ToJSON(temp_file.str());

  const char* kFileContents = "bar";
  respire::FileProcessNode create_file_node(
      &env, {}, {ebb::lib::JSONPathStringView(json_temp_file)}, {}, [&]() {
        // Do nothing on purpose.
        return optional<Error>();
      });

  respire::FileInfoNode::FuturePtr test_file_first(
      create_file_node.GetFileInfo());
  EXPECT_NE(nullptr, test_file_first->GetValue()->error());

  // Make sure that the resulting file does not exist (because of the error).
  EXPECT_FALSE(GetLastModificationTime(temp_file).has_value());
}

TEST(FileProcessNodeTest, CanChainSingleInput) {
  respire::Environment env;

  TemporaryDirectory temp_dir;;
  Path inter_file = Join(temp_dir.path(), PathStrRef("foo.txt"));
  std::string json_inter_file = ebb::lib::ToJSON(inter_file.str());
  Path final_file = Join(temp_dir.path(), PathStrRef("bar.txt"));
  std::string json_final_file = ebb::lib::ToJSON(final_file.str());

  const char* kInterFileContents = "bar";
  respire::FileProcessNode create_file_node(
      &env, {}, {ebb::lib::JSONPathStringView(json_inter_file)}, {},
      [&]() {
        WriteToFile(inter_file, kInterFileContents);
        return optional<Error>();
      });

  respire::FileProcessNode final_file_node(
      &env, {{&create_file_node, 0}}, {
          ebb::lib::JSONPathStringView(json_final_file)}, {},
      [&]() {
        ConcatenateToFile(inter_file, inter_file, final_file);
        return optional<Error>();
      });

  auto before_create = std::chrono::system_clock::now();
  respire::FileInfoNode::FuturePtr test_file1(final_file_node.GetFileInfo());

  // Ensure that the final result was built properly.
  ASSERT_NE(nullptr, test_file1->GetValue()->value());
  const respire::FileOutput::Value& value = *test_file1->GetValue()->value();
  ASSERT_EQ(1, value.size());
  EXPECT_EQ(final_file, value[0].filename.AsString());
  if (platform::kFileModificationTimeConsistentWithSystemClock) {
    EXPECT_GE(*value[0].last_modified_time, before_create);
  }
  auto last_final_modified_time1 = GetLastModificationTime(final_file);
  ASSERT_TRUE(last_final_modified_time1.has_value());
  EXPECT_EQ(*value[0].last_modified_time, *last_final_modified_time1);

  EXPECT_EQ(std::string(kInterFileContents) + std::string(kInterFileContents),
            ReadFileContents(final_file));

  // Ensure also that the intermediate file is built and exists.
  auto last_inter_modified_time1 = GetLastModificationTime(inter_file);
  ASSERT_TRUE(last_inter_modified_time1.has_value());
  if (platform::kFileModificationTimeConsistentWithSystemClock) {
    EXPECT_GE(*last_inter_modified_time1, before_create);
  }
  EXPECT_EQ(kInterFileContents, ReadFileContents(inter_file));

  // Ensure that a second pull does not update either file.
  respire::FileInfoNode::FuturePtr test_file2(final_file_node.GetFileInfo());
  ASSERT_NE(nullptr, test_file2->GetValue()->value());
  auto last_final_modified_time2 = GetLastModificationTime(final_file);
  ASSERT_TRUE(last_final_modified_time2.has_value());
  EXPECT_EQ(*last_final_modified_time1, *last_final_modified_time2);
  auto last_inter_modified_time2 = GetLastModificationTime(inter_file);
  ASSERT_TRUE(last_inter_modified_time2.has_value());
  EXPECT_EQ(*last_inter_modified_time1, *last_inter_modified_time2);
}

TEST(FileProcessNodeTest, InputErrorsArePropagated) {
  respire::Environment env;

  TemporaryDirectory temp_dir;
  Path inter_file = Join(temp_dir.path(), PathStrRef("foo.txt"));
  std::string json_inter_file = ebb::lib::ToJSON(inter_file.str());
  Path final_file = Join(temp_dir.path(), PathStrRef("bar.txt"));
  std::string json_final_file = ebb::lib::ToJSON(final_file.str());

  const char* kInterFileContents = "bar";
  respire::FileProcessNode create_file_node(
      &env, {}, {ebb::lib::JSONPathStringView(json_inter_file)}, {},
      [&]() { return Error("Made up test error!"); });

  respire::FileProcessNode final_file_node(
      &env, {{&create_file_node, 0}}, {
          ebb::lib::JSONPathStringView(json_final_file)}, {},
      [&]() {
        ConcatenateToFile(inter_file, inter_file, final_file);
        return optional<Error>();
      });

  respire::FileInfoNode::FuturePtr test_file(final_file_node.GetFileInfo());
  EXPECT_NE(nullptr, test_file->GetValue()->error());

  // Make sure that the final file does not exist (because of the error).
  EXPECT_FALSE(GetLastModificationTime(final_file).has_value());
}

TEST(FileProcessNodeTest, MultipleInputs) {
  respire::Environment env;

  TemporaryDirectory temp_dir;
  Path inter_file1 = Join(temp_dir.path(), PathStrRef("foo1.txt"));
  std::string json_inter_file1 = ebb::lib::ToJSON(inter_file1.str());
  Path inter_file2 = Join(temp_dir.path(), PathStrRef("foo2.txt"));
  std::string json_inter_file2 = ebb::lib::ToJSON(inter_file2.str());
  Path final_file = Join(temp_dir.path(), PathStrRef("bar.txt"));
  std::string json_final_file = ebb::lib::ToJSON(final_file.str());

  const char* kInterFileContents1 = "bar1";
  respire::FileProcessNode create_file_node1(
      &env, {}, {ebb::lib::JSONPathStringView(json_inter_file1)}, {},
      [&]() {
        WriteToFile(inter_file1, kInterFileContents1);
        return optional<Error>();
      });

  const char* kInterFileContents2 = "bar2";
  respire::FileProcessNode create_file_node2(
      &env, {}, {ebb::lib::JSONPathStringView(json_inter_file2)}, {},
      [&]() {
        WriteToFile(inter_file2, kInterFileContents2);
        return optional<Error>();
      });

  respire::FileProcessNode final_file_node(
      &env, {{&create_file_node1, 0}, {&create_file_node2, 0}}, {
          ebb::lib::JSONPathStringView(json_final_file)}, {},
      [&]() {
        ConcatenateToFile(inter_file1, inter_file2, final_file);
        return optional<Error>();
      });

  auto before_create = std::chrono::system_clock::now();
  respire::FileInfoNode::FuturePtr test_file1(final_file_node.GetFileInfo());

  // Ensure that the final result was built properly.
  ASSERT_NE(nullptr, test_file1->GetValue()->value());
  const respire::FileOutput::Value& value = *test_file1->GetValue()->value();
  ASSERT_EQ(1, value.size());
  EXPECT_EQ(final_file, value[0].filename.AsString());
  if (platform::kFileModificationTimeConsistentWithSystemClock) {
    EXPECT_GE(*value[0].last_modified_time, before_create);
  }
  auto last_final_modified_time = GetLastModificationTime(final_file);
  ASSERT_TRUE(last_final_modified_time.has_value());
  EXPECT_EQ(*value[0].last_modified_time, *last_final_modified_time);

  EXPECT_EQ(std::string(kInterFileContents1) + std::string(kInterFileContents2),
            ReadFileContents(final_file));

  // Ensure also that the intermediate files are built and exist.
  auto last_inter_modified_time1 = GetLastModificationTime(inter_file1);
  ASSERT_TRUE(last_inter_modified_time1.has_value());
  if (platform::kFileModificationTimeConsistentWithSystemClock) {
    EXPECT_GE(*last_inter_modified_time1, before_create);
  }
  EXPECT_EQ(kInterFileContents1, ReadFileContents(inter_file1));

  auto last_inter_modified_time2 = GetLastModificationTime(inter_file2);
  ASSERT_TRUE(last_inter_modified_time2.has_value());
  if (platform::kFileModificationTimeConsistentWithSystemClock) {
    EXPECT_GE(*last_inter_modified_time2, before_create);
  }
  EXPECT_EQ(kInterFileContents2, ReadFileContents(inter_file2));
}
