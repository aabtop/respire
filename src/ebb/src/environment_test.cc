#include <gtest/gtest.h>
#include "ebbpp.h"

TEST(EnvironmentTests, CanInitializeEnvironment) {
  ebb::Environment env(0, 16 * 1024);
}
