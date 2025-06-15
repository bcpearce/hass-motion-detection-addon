#include <gtest/gtest.h>

#include "SimServer.h"

class ServerEnvironment : public ::testing::Environment {
public:
  void SetUp() override { SimServer::Start(SIM_SERVER_PORT); }
};

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);

  auto *serverEnv = testing::AddGlobalTestEnvironment(
      std::make_unique<ServerEnvironment>().release());

  return RUN_ALL_TESTS();
}