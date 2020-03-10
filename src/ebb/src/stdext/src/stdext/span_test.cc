#include "stdext/span.h"

#include <gtest/gtest.h>

namespace stdext {

TEST(SpanTests, BasicConstructAndAccessTest) {
  int my_int = 1;
  span<int> my_span(&my_int, 1);

  EXPECT_EQ(&my_int, my_span.data());
  EXPECT_EQ(1, my_span.size());
}

TEST(SpanTests, CanCopyConstructSpans) {
  int my_int = 1;
  span<int> my_span(&my_int, 1);

  span<int> copied_span(my_span);

  EXPECT_EQ(&my_int, copied_span.data());
  EXPECT_EQ(1, copied_span.size());
}

TEST(SpanTests, RangeBasedForLoop) {
  const int kBufferSize = 10;
  int buffer[kBufferSize];
  for (int i = 0; i < kBufferSize; ++i) {
    buffer[i] = i;
  }

  stdext::span<int> int_span(buffer, kBufferSize);
  for (auto& i : int_span) {
    i++;
  }

  // Also test the const version of these functions.
  int count = 1;
  const stdext::span<int> const_span(int_span);
  for (const auto& i : const_span) {
    EXPECT_EQ(count, i);
    ++count;
  }
}
}  // namespace stdext
