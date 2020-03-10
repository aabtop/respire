#include <fstream>
#include <gtest/gtest.h>

#include "environment.h"
#include "file_exists_node.h"
#include "lib/json_string_view.h"
#include "stdext/file_system.h"

using ebb::lib::JSONPathStringView;
using stdext::file_system::Join;
using stdext::file_system::PathStrRef;
using stdext::file_system::Path;
using stdext::file_system::TemporaryDirectory;

TEST(FileExistsNodeTest, NoFileReturnsError) {
  respire::Environment env;

  TemporaryDirectory temp_dir;
  Path temp_file = Join(temp_dir.path(), PathStrRef("foo.txt"));
  std::string json_temp_file = ebb::lib::ToJSON(temp_file.str());

  respire::FileExistsNode test_node(&env, JSONPathStringView(json_temp_file));

  respire::FileInfoNode::FuturePtr test_file(test_node.GetFileInfo());

  EXPECT_NE(nullptr, test_file->GetValue()->error());
}

TEST(FileExistsNodeTest, FileExistingReturnsValidResult) {
  respire::Environment env;

  TemporaryDirectory temp_dir;
  Path temp_file1 = Join(temp_dir.path(), PathStrRef("foo1.txt"));
  std::string json_temp_file1 = ebb::lib::ToJSON(temp_file1.str());
  Path temp_file2 = Join(temp_dir.path(), PathStrRef("foo2.txt"));
  std::string json_temp_file2 = ebb::lib::ToJSON(temp_file2.str());

  respire::FileExistsNode test_node1(&env, JSONPathStringView(json_temp_file1));

  auto before_create = std::chrono::system_clock::now();

  {
    std::ofstream out(temp_file1.str());
    out << "bar1";
  }

  respire::FileInfoNode::FuturePtr test_file1(test_node1.GetFileInfo());

  ASSERT_NE(nullptr, test_file1->GetValue()->value());

  const respire::FileOutput::Value& value1 = *test_file1->GetValue()->value();
  ASSERT_EQ(1, value1.size());
  EXPECT_EQ(temp_file1, value1[0].filename.AsString());
  if (platform::kFileModificationTimeConsistentWithSystemClock) {
    EXPECT_GE(*value1[0].last_modified_time, before_create);
  }

  // Make sure a file created afterwards ends up with a later timestamp.
  respire::FileExistsNode test_node2(&env, JSONPathStringView(json_temp_file2));
  {
    std::ofstream out(temp_file2.str());
    out << "bar2";
  }

  respire::FileInfoNode::FuturePtr test_file2(test_node2.GetFileInfo());
  ASSERT_NE(nullptr, test_file1->GetValue()->value());

  const respire::FileOutput::Value& value2 = *test_file2->GetValue()->value();
  ASSERT_EQ(1, value2.size());
  EXPECT_EQ(temp_file2, value2[0].filename.AsString());
  EXPECT_GE(*value2[0].last_modified_time, *value1[0].last_modified_time);
}

TEST(FileExistsNodeTest, DirectoryExistingReturnsValidResult) {
  respire::Environment env;

  TemporaryDirectory temp_dir;
  std::string json_temp_dir = ebb::lib::ToJSON(temp_dir.path().str());
  auto json_temp_dir_view = JSONPathStringView(json_temp_dir);

  Path temp_file1 = Join(temp_dir.path(), PathStrRef("foo1.txt"));
  Path temp_file2 = Join(temp_dir.path(), PathStrRef("foo2.txt"));

  auto last_modified_time = std::chrono::system_clock::now();

  respire::FileExistsNode test_node(&env, json_temp_dir_view);

  respire::FileInfoNode::FuturePtr test_result1(test_node.GetFileInfo());
  ASSERT_NE(nullptr, test_result1->GetValue()->value());

  const respire::FileOutput::Value& value1 = *test_result1->GetValue()->value();
  ASSERT_EQ(1, value1.size());
  EXPECT_EQ(json_temp_dir_view.AsString(), value1[0].filename.AsString());
  if (platform::kFileModificationTimeConsistentWithSystemClock) {
    EXPECT_GE(*value1[0].last_modified_time, last_modified_time);
  }
  last_modified_time = *value1[0].last_modified_time;

  // Test that creating files triggers a directory modification.
  {
    std::ofstream out(temp_file1.str());
    out << "bar1";
  }

  respire::FileInfoNode::FuturePtr test_result2(test_node.GetFileInfo());
  ASSERT_NE(nullptr, test_result2->GetValue()->value());

  const respire::FileOutput::Value& value2 = *test_result2->GetValue()->value();
  ASSERT_EQ(1, value2.size());
  EXPECT_EQ(json_temp_dir_view.AsString(), value2[0].filename.AsString());
  EXPECT_GE(*value2[0].last_modified_time, last_modified_time);
  last_modified_time = *value2[0].last_modified_time;

  {
    std::ofstream out(temp_file2.str());
    out << "bar2";
  }

  respire::FileInfoNode::FuturePtr test_result3(test_node.GetFileInfo());
  ASSERT_NE(nullptr, test_result3->GetValue()->value());

  const respire::FileOutput::Value& value3 = *test_result3->GetValue()->value();
  ASSERT_EQ(1, value3.size());
  EXPECT_EQ(json_temp_dir_view.AsString(), value3[0].filename.AsString());
  EXPECT_GE(*value3[0].last_modified_time, last_modified_time);
  last_modified_time = *value3[0].last_modified_time;

  // Make sure the modified time stays the same if nothing changes.
  respire::FileInfoNode::FuturePtr test_result4(test_node.GetFileInfo());
  ASSERT_NE(nullptr, test_result4->GetValue()->value());

  const respire::FileOutput::Value& value4 = *test_result4->GetValue()->value();
  EXPECT_GE(*value4[0].last_modified_time, last_modified_time);
  last_modified_time = *value4[0].last_modified_time;

  // Test that file removal triggers a directory modification.
  remove(temp_file1.c_str());

  respire::FileInfoNode::FuturePtr test_result5(test_node.GetFileInfo());
  ASSERT_NE(nullptr, test_result5->GetValue()->value());

  const respire::FileOutput::Value& value5 = *test_result5->GetValue()->value();
  ASSERT_EQ(1, value5.size());
  EXPECT_EQ(json_temp_dir_view.AsString(), value5[0].filename.AsString());
  EXPECT_GE(*value5[0].last_modified_time, last_modified_time);
  last_modified_time = *value5[0].last_modified_time;  
}
