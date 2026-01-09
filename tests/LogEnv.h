#pragma once
#include "Logger.h"

class LoggerEnvironment : public ::testing::Environment {
public:
  void SetUp() override {
    auto stdoutLogger = logger::InitStdoutLogger();
    auto stderrLogger = logger::InitStderrLogger();
  }
  void TearDown() override {}
};