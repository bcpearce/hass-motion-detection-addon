#include <gtest/gtest.h>

#include "Logger.h"

#include "SimServer.h"

namespace {

class ServerEnvironment : public ::testing::Environment {
public:
  void SetUp() override { SimServer::Start(SIM_SERVER_PORT); }
  void TearDown() override { SimServer::Stop(); }
};

class LoggerEnvironment : public ::testing::Environment {
public:
  void SetUp() override {
    auto stdoutLogger = logger::InitStdoutLogger();
    auto stderrLogger = logger::InitStderrLogger();
  }
  void TearDown() override {}
};

} // namespace

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);

  auto *loggerEnv = testing::AddGlobalTestEnvironment(
      std::make_unique<LoggerEnvironment>().release());
  auto *serverEnv = testing::AddGlobalTestEnvironment(
      std::make_unique<ServerEnvironment>().release());

  return RUN_ALL_TESTS();
}