#include <cstring>
#include <gtest/gtest.h>

#include "ebbpp.h"

TEST(QueueTests, CanInitializeQueue) {
  ebb::Environment env(0, 0);
  ebb::QueueWithMemory<char, 1> queue(&env);
}

TEST(QueueTests, CanPushPull) {
  ebb::Environment env(0, 0);
  ebb::QueueWithMemory<char, 1> queue(&env);

  const char kTestData = 'A';
  {
    ebb::Push<char> push(&queue, kTestData);
  }

  {
    ebb::Pull<char> pull(&queue);
    EXPECT_EQ(kTestData, *pull.data());
  }
}

TEST(QueueTests, CanPushPullVoid) {
  ebb::Environment env(0, 0);
  ebb::QueueWithMemory<void, 1> queue(&env);

  {
    ebb::Push<>{&queue};
  }

  {
    ebb::Pull<>{&queue};
  }

  // Nothing to test here, besides the fact that we don't deadlock on the
  // call to Pull(), and to make sure that the code compiles with no parameters
  // passed in.
}

TEST(QueueTests, CanPushPullMultipleStructs) {
  struct TestStruct {
    int32_t a;
    int8_t b;
  };

  ebb::Environment env(0, 0);
  ebb::QueueWithMemory<TestStruct, 2> queue(&env);

  const TestStruct kItem1 = {32000, 10};
  const TestStruct kItem2 = {128000, 20};

  {
    ebb::Push<TestStruct> push(&queue, kItem1);
  }

  {
    ebb::Push<TestStruct> push(&queue, kItem2);
  }

  {
    ebb::Pull<TestStruct> pull(&queue);
    EXPECT_EQ(0, memcmp(&kItem1, pull.data(), sizeof(kItem1)));
  }

  {
    ebb::Pull<TestStruct> pull(&queue);
    EXPECT_EQ(0, memcmp(&kItem2, pull.data(), sizeof(kItem2)));
  }
}
