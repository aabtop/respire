#include "platform/context.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

using platform::Context;
using platform::SwitchToContext;
using platform::SwitchToNewContext;
using platform::GetContextData;

const int kDefaultStackSize = 16 * 1024;

TEST(ContextTests, CanMakeAndReturnFromContext) {
  Context* prev_context =
      SwitchToNewContext(kDefaultStackSize, nullptr, [](Context* context) {
        return context;
      });
  EXPECT_EQ(nullptr, prev_context);
}

TEST(ContextTests, CanMakeAndRunCodeFromContext) {
  int foo = 0;

  Context* prev_context =
      SwitchToNewContext(kDefaultStackSize, nullptr, [&foo](Context* context) {
        foo = 1;
        return context;
      });

  EXPECT_EQ(nullptr, prev_context);
  EXPECT_EQ(foo, 1);
}

TEST(ContextTests, CanPassDataToContext) {
  int foo = 0;

  Context* prev_context =
      SwitchToNewContext(kDefaultStackSize, &foo, [](Context* context) {
        int* data_int = reinterpret_cast<int*>(GetContextData(context));
        *data_int = 1;
        return context;
      });

  EXPECT_EQ(nullptr, prev_context);
  EXPECT_EQ(foo, 1);
}

TEST(ContextTests, SmallSwitchStack) {
  int foo = 0;

  Context* prev_context =
      SwitchToNewContext(kDefaultStackSize, nullptr, [&foo](Context* context) {
        EXPECT_EQ(0, foo);
        foo = 1;
        Context* prev_context = SwitchToNewContext(
            kDefaultStackSize, nullptr, [&foo](Context* context) {
              EXPECT_EQ(1, foo);
              foo = 2;
              return context;
            });
        EXPECT_EQ(nullptr, prev_context);
        EXPECT_EQ(2, foo);
        foo = 3;
        return context;
      });
  EXPECT_EQ(nullptr, prev_context);
  EXPECT_EQ(3, foo);
}

TEST(ContextTests, CoopFiberPair) {
  int foo = 0;

  Context* helper_context =
      SwitchToNewContext(kDefaultStackSize, nullptr,
                         [&foo](Context* main_context) {
        EXPECT_EQ(0, foo);
        foo = 1;
        main_context = SwitchToContext(main_context, nullptr);
        EXPECT_EQ(2, foo);
        foo = 3;
        main_context = SwitchToContext(main_context, nullptr);
        EXPECT_EQ(4, foo);
        foo = 5;
        return main_context;
      });

  EXPECT_EQ(1, foo);
  foo = 2;
  helper_context = SwitchToContext(helper_context, nullptr);
  EXPECT_EQ(3, foo);
  foo = 4;
  helper_context = SwitchToContext(helper_context, nullptr);
  EXPECT_EQ(5, foo);
  EXPECT_EQ(nullptr, helper_context);
}

TEST(ContextTests, SmallSwitchRing) {
  const int kRingSize = 5;

  int foo = 0;
  Context* main_context;

  std::vector<Context*> ring_contexts;
  std::vector<bool> initialized(kRingSize, false);

  platform::FiberMain ring_function = [&](Context* context) {
    int my_id = foo;
    ++foo;

    if (my_id == 0) {
      main_context = context;
    } else {
      ring_contexts.push_back(context);
    }

    EXPECT_FALSE(initialized[my_id]);
    initialized[my_id] = true;

    if (foo < kRingSize) {
      Context* prev_context =
          SwitchToNewContext(kDefaultStackSize, nullptr, ring_function);
      ring_contexts.push_back(prev_context);
    } else {
      Context* next_context = ring_contexts.front();
      ring_contexts.erase(ring_contexts.begin());
      Context* prev_context = SwitchToContext(next_context, nullptr);
      ring_contexts.push_back(prev_context);
    }

    EXPECT_EQ(my_id + kRingSize, foo);
    ++foo;

    Context* next_context = ring_contexts.front();
    ring_contexts.erase(ring_contexts.begin());
    Context* prev_context = SwitchToContext(next_context, nullptr);
    if (prev_context) {
      EXPECT_EQ(0, my_id);
      ring_contexts.push_back(prev_context);
    } else {
      EXPECT_NE(0, my_id);
    }

    EXPECT_EQ(my_id + kRingSize * 2, foo);
    ++foo;

    if (!ring_contexts.empty()) {
      Context* final_context = ring_contexts.front();
      ring_contexts.erase(ring_contexts.begin());

      return final_context;
    } else {
      return main_context;
    }
  };

  Context* prev_context =
      SwitchToNewContext(kDefaultStackSize, nullptr, ring_function);
  EXPECT_EQ(nullptr, prev_context);
  EXPECT_EQ(kRingSize * 3, foo);
}