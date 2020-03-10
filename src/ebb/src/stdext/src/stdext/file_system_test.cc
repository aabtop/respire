#include "stdext/file_system.h"

#include <fstream>
#include <gtest/gtest.h>
#include <unordered_set>

using stdext::file_system::Join;
using stdext::file_system::PathStrRef;
using stdext::file_system::Path;

TEST(FileSystemTests, CanConstructFilePathStrRef) {
  const char *kTestFileName = "foobar";
  PathStrRef file_path(kTestFileName);

  EXPECT_EQ(kTestFileName, file_path.c_str());
}

TEST(FileSystemTests, CanConstructFilePath) {
  const char *kTestFileName = "foobar";
  Path file_path(kTestFileName);

  EXPECT_NE(kTestFileName, file_path.c_str());
  EXPECT_EQ(std::string(kTestFileName), std::string(file_path.c_str()));
  EXPECT_EQ(std::string(kTestFileName), file_path.str());
}

TEST(FileSYstemTests, CanPopFilePath) {
  auto joined_path = Join(Join("foo", "bar"), "cow.exe");
  EXPECT_EQ(Join("foo", "bar"), joined_path.pop().str());
  EXPECT_EQ("foo", joined_path.pop().pop().str());
}

template <typename T>
class PathTypeAgnosticTest : public ::testing::Test {};

TYPED_TEST_CASE_P(PathTypeAgnosticTest);

TYPED_TEST_P(PathTypeAgnosticTest, CanJoin) {
  TypeParam path_a("foo");
  TypeParam path_b("bar");

  Path result = Join(path_a, path_b);

  EXPECT_EQ(std::string("foo") + platform::kPathSeparator + "bar",
            result.str());
}

REGISTER_TYPED_TEST_CASE_P(
    PathTypeAgnosticTest,
    CanJoin);

typedef ::testing::Types<PathStrRef, Path> PathTypes;
INSTANTIATE_TYPED_TEST_CASE_P(A, PathTypeAgnosticTest, PathTypes);

TEST(FileSystemTests, GetLastModificationTimeOnMissingFile) {
  stdext::file_system::TemporaryDirectory temp_dir;
  Path temp_file = Join(temp_dir.path(), PathStrRef("foo.txt"));

  auto modify_time = stdext::file_system::GetLastModificationTime(temp_file);
  EXPECT_FALSE(modify_time.has_value());
}

TEST(FileSystemTests, GetLastModificationTimeWorksOnCreatedDirectories) {
  auto before_create = std::chrono::system_clock::now();

  stdext::file_system::TemporaryDirectory temp_dir;

  auto after_create = std::chrono::system_clock::now();

  auto modify_time =
      stdext::file_system::GetLastModificationTime(temp_dir.path());
  ASSERT_TRUE(modify_time.has_value());
  if (platform::kFileModificationTimeConsistentWithSystemClock) {
    EXPECT_GE(*modify_time, before_create);
    EXPECT_LE(*modify_time, after_create);
  }
}

TEST(FileSystemTests, GetLastModificationTimeWorksOnCreatedFiles) {
  stdext::file_system::TemporaryDirectory temp_dir;
  Path temp_file = Join(temp_dir.path(), PathStrRef("foo.txt"));

  auto before_create = std::chrono::system_clock::now();

  {
    std::ofstream out(temp_file.str());
    out << "bar";
  }

  auto after_create = std::chrono::system_clock::now();

  auto modify_time = stdext::file_system::GetLastModificationTime(temp_file);
  ASSERT_TRUE(modify_time.has_value());
  if (platform::kFileModificationTimeConsistentWithSystemClock) {
    EXPECT_GE(*modify_time, before_create);
    EXPECT_LE(*modify_time, after_create);
  }
}

TEST(FileSystemTests, GetLastModificationTimeWorksOnModifiedDir) {
  stdext::file_system::TemporaryDirectory temp_dir;
  Path temp_file = Join(temp_dir.path(), PathStrRef("foo.txt"));

  auto before_create = std::chrono::system_clock::now();

  {
    std::ofstream out(temp_file.str());
    out << "bar";
  }

  auto after_create = std::chrono::system_clock::now();

  auto modify_time =
      stdext::file_system::GetLastModificationTime(temp_dir.path());
  ASSERT_TRUE(modify_time.has_value());
  if (platform::kFileModificationTimeConsistentWithSystemClock) {
    EXPECT_GE(*modify_time, before_create);
    EXPECT_LE(*modify_time, after_create);
  }
}

TEST(FileSystemTests, GetLastModificationTimeWorksOnModifiedFiles) {
  stdext::file_system::TemporaryDirectory temp_dir;
  Path temp_file = Join(temp_dir.path(), PathStrRef("foo.txt"));

  {
    std::ofstream out(temp_file.str());
    out << "bar";
  }

  auto before_modify_now = std::chrono::system_clock::now();
  auto create_time = stdext::file_system::GetLastModificationTime(temp_file);
  ASSERT_TRUE(create_time.has_value());
  if (platform::kFileModificationTimeConsistentWithSystemClock) {
    EXPECT_GE(before_modify_now, *create_time);
  }

  {
    std::ofstream out(temp_file.str(), std::ofstream::app);
    out << "bosa";
  }

  auto after_modify_now = std::chrono::system_clock::now();

  auto modify_time = stdext::file_system::GetLastModificationTime(temp_file);
  ASSERT_TRUE(modify_time.has_value());
  EXPECT_GE(*modify_time, *create_time);
  if (platform::kFileModificationTimeConsistentWithSystemClock) {
    EXPECT_GE(*modify_time, before_modify_now);
    EXPECT_LE(*modify_time, after_modify_now);
  }
}

TEST(FileSystemTests, CanUsePathInUnorderedSet) {
  std::unordered_set<Path> path_set;
  path_set.insert(Path("foo.txt"));

  EXPECT_NE(path_set.end(), path_set.find(Path("foo.txt")));
  EXPECT_EQ(path_set.end(), path_set.find(Path("bar.txt")));

  // Make sure that we can also lookup with PathStrRef objects.
  EXPECT_NE(path_set.end(), path_set.find(PathStrRef("foo.txt")));
  EXPECT_EQ(path_set.end(), path_set.find(PathStrRef("bar.txt")));
}

TEST(FileSystemTests, CanGetSelfModuleFilePath) {
  auto this_exe_modification_time =
      stdext::file_system::GetLastModificationTime(
          stdext::file_system::GetThisModulePath());
  EXPECT_TRUE(this_exe_modification_time.has_value());
}
