#pragma once
#include "SimServer.h"

class ServerEnvironment : public ::testing::Environment {
public:
  void SetUp() override { SimServer::Start(SIM_SERVER_PORT); }
  void TearDown() override { SimServer::Stop(); }
};