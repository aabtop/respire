#include <cstring>
#include <gtest/gtest.h>

#include "ebbpp.h"

class ConsumerTests : public ::testing::TestWithParam<int32_t> {};

const size_t kFiberStackSize = 16 * 1024;

TEST_P(ConsumerTests, CanConsumeData) {
  ebb::Environment env(GetParam(), kFiberStackSize);
  ebb::QueueWithMemory<char, 1> queue(&env);

  const char kTestData = 'A';

  char received_data = '\0';

  {
    ebb::Consumer<char> consumer(&env, &queue, [&received_data](char data) {
      received_data = data;
    });

    {
      ebb::Push<char> push(&queue, kTestData);
    }
  }  // Destructing the consumer should flush its queue.

  EXPECT_EQ(kTestData, received_data);
}

TEST_P(ConsumerTests, CanConsumeMoreDataThanQueueSize) {
  ebb::Environment env(GetParam(), kFiberStackSize);
  ebb::QueueWithMemory<char, 1> queue(&env);

  const char kTestData[] = "Hello there.";

  char received_data[sizeof(kTestData)] = {0};
  int num_data_received = 0;
  {
    ebb::Consumer<char> consumer(
        &env, &queue, [&received_data, &num_data_received](char data) {
      received_data[num_data_received++] = data;
    });

    for (size_t i = 0; i < sizeof(kTestData); ++i) {
      ebb::Push<char> push(&queue, kTestData[i]);
    }
  }  // Destructing the consumer should flush its queue.

  ASSERT_EQ(sizeof(kTestData), num_data_received);
  EXPECT_EQ(0, memcmp(&kTestData, received_data, sizeof(kTestData)));
}

template<typename T>
class QueueDynamicMemory {
 public:
  QueueDynamicMemory(ebb::Environment* env, size_t max_items)
      : queue_memory_(max_items)
      , queue_(env, queue_memory_.data(), queue_memory_.size()) {}

  ebb::QueueGivenMemory<T>* queue() { return &queue_; }
 
 private:
  std::vector<T> queue_memory_;
  ebb::QueueGivenMemory<T> queue_;
};

template<typename T>
class ConsumerWithDynamicQueue {
 public:
  ConsumerWithDynamicQueue(ebb::Environment* env, size_t max_items,
                           const std::function<void(T&&)>& consumer_function)
      : queue_(env, max_items),
        consumer_(env, queue_.queue(), consumer_function) {}

  ebb::Queue<T>* queue() { return queue_.queue(); }

 private:
  QueueDynamicMemory<T> queue_;
  ebb::Consumer<T> consumer_;
};

template <typename T>
class ConsumerChain {
 public:
  ConsumerChain(ebb::Environment* env, int num_consumers, int queue_lengths,
                ebb::Queue<T>* output_queue,
                const std::function<void(T*)>& process_function)
      : process_function_(process_function) {
    for (int i = 0; i < num_consumers; ++i) {
      ebb::Queue<T>* step_output_queue =
          i == 0 ? output_queue : consumers_[i - 1]->queue();

      int queue_length = queue_lengths;
      if (i == num_consumers - 1) {
        queue_length = 100;
      }
      consumers_.emplace_back(new ConsumerWithDynamicQueue<T>(
          env, queue_length, [step_output_queue, this](T&& data) {
        // Process the function and then pass it on to the next consumer.
        process_function_(&data);
        ebb::Push<T> push(step_output_queue, std::move(data));
      }));
    }
  }

  ~ConsumerChain() {
    // Ensure that consumers are cleaned up in reverse order.
    while (!consumers_.empty()) {
      consumers_.pop_back();
    }
  }

  ebb::Queue<T>* input_queue() {
    return consumers_.back()->queue();
  }

 private:
  std::function<void(T*)> process_function_;
  std::vector<std::unique_ptr<
      ConsumerWithDynamicQueue<T>>> consumers_;
};

INSTANTIATE_TEST_CASE_P(
    WithDifferentThreadPoolsSizes, ConsumerTests, ::testing::Values(
        1, 2, 8, 100));

struct ConsumerChainTestParams {
  int num_threads;
  int num_items_to_push;
  int internal_queue_size;
  int chain_length;
};

class ConsumerChainTests :
    public ::testing::TestWithParam<ConsumerChainTestParams> {};

TEST_P(ConsumerChainTests, ConsumerChainFlowsCorrectly) {
  ebb::Environment env(GetParam().num_threads, kFiberStackSize);
  std::vector<int> initial_values(GetParam().num_items_to_push);
  for (int i = 0; i < GetParam().num_items_to_push; ++i) {
    initial_values[i] = i;
  }
  std::vector<int32_t> output_queue_data(GetParam().num_items_to_push);
  ebb::QueueGivenMemory<int32_t> output_queue(
      &env, output_queue_data.data(), output_queue_data.size());

  {
    ConsumerChain<int32_t> chain(
        &env, GetParam().chain_length, GetParam().internal_queue_size,
        &output_queue, [](int32_t* value) {
          (*value) += 1;
          std::this_thread::yield();
        });

    for (int i : initial_values) {
      ebb::Push<int32_t> push(chain.input_queue(), i);
    }
  }

  for (int i : initial_values) {
    ebb::Pull<int32_t> pull(&output_queue);
    EXPECT_EQ(i + GetParam().chain_length, *pull.data());
  }
}

ConsumerChainTestParams kConsumerChainTestParams[] = {
  {1, 20, 20, 200},
  {2, 20, 20, 200},
  {8, 20, 20, 200},
  {100, 20, 20, 200},
  {1, 100, 100, 200},
  {2, 100, 100, 200},
  {8, 100, 100, 200},
  {100, 100, 100, 200},
  // By setting the internal queue size to 1, we force some context
  // switching since processed items are no longer able to "pile up" on
  // a consumer.
  {1, 20, 1, 200},
  {2, 20, 1, 200},
  {8, 20, 1, 200},
  {8, 100, 1, 200},
  {100, 20, 1, 200},
};

INSTANTIATE_TEST_CASE_P(
    VaryingConsumerChainParams, ConsumerChainTests,
    ::testing::ValuesIn(kConsumerChainTestParams));

TEST(SimpleConsumerTest, CanMakeVoidConsumer) {
  // Make sure that <void> consumers work properly with 0 parameter pushes.
  ebb::Environment env(1, kFiberStackSize);
  ebb::QueueWithMemory<void, 1> queue(&env);

  int received_counter = 0;

  {
    ebb::Consumer<void> consumer(&env, &queue, [&received_counter]() {
      ++received_counter;
    });

    ebb::Push<>{&queue};
  }
  EXPECT_EQ(1, received_counter);

  {
    ebb::Consumer<void> consumer(&env, &queue, [&received_counter]() {
      ++received_counter;
    });

    {
      ebb::Push<>{&queue};
    }
    {
      ebb::Push<>{&queue};
    }    
  }
  EXPECT_EQ(3, received_counter);
}