#pragma once
#include "SimServer.h"
#include <cstdlib>

class ServerEnvironment : public ::testing::Environment {
public:
  void SetUp() override {
    if (const auto *port = std::getenv("SIM_SERVER_PORT")) {
      SimServer::Start(std::stoi(port));
    } else {
      SimServer::Start(SIM_SERVER_PORT);
    }
  }
  void TearDown() override { SimServer::Stop(); }
};