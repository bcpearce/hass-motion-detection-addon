#pragma once
#include "SimServer.h"
#include <cstdlib>

class ServerEnvironment : public ::testing::Environment {
public:
  void SetUp() override { SimServer::Start(SIM_SERVER_PORT); }
  void TearDown() override { SimServer::Stop(); }
};