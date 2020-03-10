#include "stdext/variant.h"

#include <gtest/gtest.h>

namespace stdext {

TEST(VariantTests, TestSingleTypeVariant) {
  variant<int> test(in_place_type_t<int>(), 1);
  ASSERT_TRUE(holds_alternative<int>(test));
  EXPECT_EQ(1, get<int>(test));
}

TEST(VariantTest, TestIntOrDoubleVariantInt) {
  variant<int, double> test(in_place_type_t<int>(), 1);
  EXPECT_FALSE(holds_alternative<double>(test));
  ASSERT_TRUE(holds_alternative<int>(test));
  EXPECT_EQ(1, get<int>(test));
}

TEST(VariantTest, TestIntOrDoubleVariantDouble) {
  variant<int, double> test(in_place_type_t<double>(), 2.0);
  EXPECT_FALSE(holds_alternative<int>(test));
  ASSERT_TRUE(holds_alternative<double>(test));
  EXPECT_EQ(2.0, get<double>(test));
}

TEST(VariantTest, ConstructWithoutInPlaceTypeInt) {
  variant<int, double> test(1);
  EXPECT_FALSE(holds_alternative<double>(test));
  ASSERT_TRUE(holds_alternative<int>(test));
  EXPECT_EQ(1, get<int>(test));
}

TEST(VariantTest, ConstructWithoutInPlaceTypeDouble) {
  variant<int, double> test(2.0);
  EXPECT_FALSE(holds_alternative<int>(test));
  ASSERT_TRUE(holds_alternative<double>(test));
  EXPECT_EQ(2.0, get<double>(test));
}

class WasCopiedOrMoved {
 public:
  WasCopiedOrMoved() : moved_(false), copied_(false) {}
  WasCopiedOrMoved(const WasCopiedOrMoved& rhs)
      : moved_(false), copied_(true) {}
  WasCopiedOrMoved(WasCopiedOrMoved&& rhs) : moved_(true), copied_(false) {}

  bool moved() const { return moved_; }
  bool copied() const { return copied_; }

 private:
  bool moved_;
  bool copied_;
};

TEST(VariantTest, CanCopyVariant) {
  variant<int, WasCopiedOrMoved> test1(WasCopiedOrMoved{});
  variant<int, WasCopiedOrMoved> other1(test1);

  ASSERT_TRUE(holds_alternative<WasCopiedOrMoved>(other1));
  EXPECT_FALSE(get<WasCopiedOrMoved>(other1).moved());
  EXPECT_TRUE(get<WasCopiedOrMoved>(other1).copied());

  variant<int, WasCopiedOrMoved> test2(2);
  variant<int, WasCopiedOrMoved> other2(test2);

  ASSERT_TRUE(holds_alternative<int>(other2));
  EXPECT_EQ(2, get<int>(other2));
}

TEST(VariantTest, CanMoveVariant) {
  variant<int, WasCopiedOrMoved> test1(WasCopiedOrMoved{});
  variant<int, WasCopiedOrMoved> other1(std::move(test1));

  ASSERT_TRUE(holds_alternative<WasCopiedOrMoved>(other1));
  EXPECT_TRUE(get<WasCopiedOrMoved>(other1).moved());
  EXPECT_FALSE(get<WasCopiedOrMoved>(other1).copied());

  variant<int, WasCopiedOrMoved> test2(2);
  variant<int, WasCopiedOrMoved> other2(std::move(test2));

  ASSERT_TRUE(holds_alternative<int>(other2));
  EXPECT_EQ(2, get<int>(other2));
}

TEST(VariantTest, TestEquals) {
  variant<int, double> test(2.0);
  EXPECT_FALSE(test == 2);
  EXPECT_FALSE(test == 3.0);
  EXPECT_TRUE(test == 2.0);
  EXPECT_FALSE(2 == test);
  EXPECT_FALSE(3.0 == test);
  EXPECT_TRUE(2.0 == test);

  variant<int, double> v2i(2);
  variant<int, double> v3d(3.0);
  variant<int, double> v2d(2.0);

  EXPECT_FALSE(test == v2i);
  EXPECT_FALSE(test == v3d);
  EXPECT_TRUE(test == v2d);
  EXPECT_FALSE(v2i == test);
  EXPECT_FALSE(v3d == test);
  EXPECT_TRUE(v2d == test);
}

class IncrementValueOnDestruction {
 public:
  IncrementValueOnDestruction(int* value) : value_(value) {}
  ~IncrementValueOnDestruction() { *value_ += 1; }

 private:
  int* value_;
};

TEST(VariantTest, DestructorCalled) {
  int value = 0;

  {
    variant<int, IncrementValueOnDestruction> v(
        in_place_type_t<IncrementValueOnDestruction>(), &value);

    EXPECT_EQ(0, value);
  }

  EXPECT_EQ(1, value);
}

TEST(VariantTest, DestructorNotCalledWhenNotInstantiated) {
  // No checks are done in this test, it's mostly just ensures that things
  // compile and run without crashing.
  variant<int, IncrementValueOnDestruction> v(in_place_type_t<int>(), 1);
  EXPECT_TRUE(holds_alternative<int>(v));
  EXPECT_EQ(1, get<int>(v));
}
}  // namespace stdext
