#include <gtest/gtest.h>

#include "Logger.h"

#include "SimServer.h"

namespace {

class ServerEnvironment : public ::testing::Environment {
public:
  void SetUp() override { SimServer::Start(SIM_SERVER_PORT); }
};

class LoggerEnvironment : public ::testing::Environment {
public:
  void SetUp() override {
    logger::InitStderrLogger();
    logger::InitStdoutLogger();
  }
};

} // namespace

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);

  auto *serverEnv = testing::AddGlobalTestEnvironment(
      std::make_unique<ServerEnvironment>().release());
  auto *loggerEnv = testing::AddGlobalTestEnvironment(
      std::make_unique<LoggerEnvironment>().release());

  return RUN_ALL_TESTS();
}