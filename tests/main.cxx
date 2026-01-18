#include <gtest/gtest.h>

#include "LogEnv.h"
#include "ServerEnv.h"

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);

  testing::AddGlobalTestEnvironment(
      std::make_unique<LoggerEnvironment>().release());
  testing::AddGlobalTestEnvironment(
      std::make_unique<ServerEnvironment>().release());

  return RUN_ALL_TESTS();
}