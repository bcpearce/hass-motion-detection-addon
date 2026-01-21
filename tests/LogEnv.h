#pragma once
#include "Logger.h"
#include <gtest/gtest.h>

class LoggerEnvironment : public ::testing::Environment {
public:
  void SetUp() override {
    auto stdoutLogger = logger::InitStdoutLogger();
    auto stderrLogger = logger::InitStderrLogger();
    spdlog::set_pattern("[%=10l] %v");
  }
  void TearDown() override {}
};