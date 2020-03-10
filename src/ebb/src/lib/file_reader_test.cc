#include "lib/file_reader.h"

#include <fstream>
#include <gtest/gtest.h>

#include "ebbpp.h"
#include "stdext/file_system.h"

namespace ebb {
namespace lib {

namespace {
const int kDefaultBufferSize = 512;
const int kDefaultThreadCount = 1;
const int kDefaultStackSize = 16 * 1024;
}  // internal

TEST(FileReaderTest, CanReadSmallFile) {
  stdext::file_system::TemporaryDirectory temp_dir;
  stdext::file_system::Path temp_file =
      Join(temp_dir.path(), stdext::file_system::PathStrRef("foo.txt"));

  const std::string kData("bar");
  {
    std::ofstream out(temp_file.str());
    out << kData;
  }

  ebb::Environment env(kDefaultThreadCount, kDefaultStackSize);

  BatchedQueueWithMemory<ErrorOrUInts, kDefaultBufferSize> output_queue(&env);
  FileReader reader(&env, temp_file, &output_queue);

  std::string read_back;
  do {
    ebb::Pull<ErrorOrUInts> pull(output_queue.queue());
    if (read_back.size() < kData.size()) {
      ASSERT_TRUE(
          stdext::holds_alternative<stdext::span<uint8_t>>(*pull.data()));
      auto incoming = stdext::get<stdext::span<uint8_t>>(*pull.data());
      read_back.append(
          reinterpret_cast<char*>(incoming.data()), incoming.size());
    } else {
      ASSERT_TRUE(
          stdext::holds_alternative<int>(*pull.data()));
      EXPECT_EQ(0, stdext::get<int>(*pull.data()));
      break;
    }
  } while(true);
  EXPECT_EQ(kData, read_back);
}

TEST(FileReaderTest, CanReadLargeMultiBufferFile) {
  stdext::file_system::TemporaryDirectory temp_dir;
  stdext::file_system::Path temp_file =
      Join(temp_dir.path(), stdext::file_system::PathStrRef("foo.txt"));

  const int kDataSize = kDefaultBufferSize * 100;
  std::ostringstream oss;
  for (int i = 0; i < kDataSize; ++i) {
    oss << "A";
  }
  const std::string kData(oss.str());
  {
    std::ofstream out(temp_file.str());
    out << kData;
  }

  ebb::Environment env(kDefaultThreadCount, kDefaultStackSize);

  BatchedQueueWithMemory<ErrorOrUInts, kDefaultBufferSize> output_queue(&env);
  FileReader reader(&env, temp_file, &output_queue);

  std::string read_back;
  do {
    ebb::Pull<ErrorOrUInts> pull(output_queue.queue());
    if (read_back.size() < kData.size()) {
      ASSERT_TRUE(
          stdext::holds_alternative<stdext::span<uint8_t>>(*pull.data()));
      auto incoming = stdext::get<stdext::span<uint8_t>>(*pull.data());
      read_back.append(
          reinterpret_cast<char*>(incoming.data()), incoming.size());
    } else {
      ASSERT_TRUE(
          stdext::holds_alternative<int>(*pull.data()));
      EXPECT_EQ(0, stdext::get<int>(*pull.data()));
      break;
    }
  } while(true);
  EXPECT_EQ(kData, read_back);
}

TEST(FileReaderTest, SimpleFileErrors) {
  stdext::file_system::TemporaryDirectory temp_dir;
  stdext::file_system::Path temp_file =
      Join(temp_dir.path(), stdext::file_system::PathStrRef("foo.txt"));

  ebb::Environment env(kDefaultThreadCount, kDefaultStackSize);

  BatchedQueueWithMemory<ErrorOrUInts, kDefaultBufferSize> output_queue(&env);
  FileReader reader(&env, temp_file, &output_queue);

  {
    ebb::Pull<ErrorOrUInts> pull(output_queue.queue());
    ASSERT_TRUE(
        stdext::holds_alternative<int>(*pull.data()));
    EXPECT_NE(0, stdext::get<int>(*pull.data()));
  }
}

}  // namespace lib
}  // namespace ebb