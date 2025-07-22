#include "WindowsWrapper.h"

#ifdef __linux__
#include <stdlib.h>
#endif

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "Util/ProgramOptions.h"

using namespace std::string_literals;
using namespace std::string_view_literals;

TEST(ProgramOptionsTests, Parse1) {
  std::vector<const char *> argv{"MotionDetection", "--source-url",
                                 "rtsp://feed.example.com:554"};
  const util::ProgramOptions progOpts = std::get<util::ProgramOptions>(
      util::ProgramOptions::ParseOptions(argv.size(), argv.data()));
  EXPECT_EQ(progOpts.feeds.at(0).sourceUrl.c_str(),
            "rtsp://feed.example.com:554"sv);
}

TEST(ProgramOptionsTests, ParseDetectionSize) {
  std::vector<const char *> argv{"MotionDetection", "--source-url",
                                 "rtsp://feed.example.com:554",
                                 "--detection-size", "32.045%"};
  const util::ProgramOptions progOpts = std::get<util::ProgramOptions>(
      util::ProgramOptions::ParseOptions(argv.size(), argv.data()));
  EXPECT_THAT(
      progOpts.feeds.at(0).detectionSize,
      testing::VariantWith<double>(testing::DoubleNear(0.32045, 0.001)));
}

TEST(ProgramOptionsTests, CanSetupHass) {
  static const char *argvFalse[3] = {PROJECT_NAME, "--source-url",
                                     "rtsp://feed.example.com:554"};
  const auto progOptsFalse = std::get<util::ProgramOptions>(
      util::ProgramOptions::ParseOptions(3, argvFalse));
  EXPECT_FALSE(progOptsFalse.CanSetupHass(progOptsFalse.feeds.at(0)))
      << "Options " << argvFalse[0] << " " << argvFalse[1] << " "
      << argvFalse[2] << " will set up Hass endpoint, but should not";

  static const char *argvTrue[9] = {PROJECT_NAME,
                                    "--source-url",
                                    "rtsp://feed.example.com:554",
                                    "--hass-url",
                                    "https://hass.example.com",
                                    "--hass-entity-id",
                                    "binary_sensor.hello",
                                    "--hass-token",
                                    "abcd"};

  const auto progOptsTrue = std::get<util::ProgramOptions>(
      util::ProgramOptions::ParseOptions(9, argvTrue));
  EXPECT_TRUE(progOptsTrue.CanSetupHass(progOptsTrue.feeds.at(0)));
}

TEST(ProgramOptionsTests, CanSetupHassWithEnvironmentVariables) {
#if _WIN32
  EXPECT_EQ(
      0, _putenv_s("MODET_HASS_URL", "https://homeassistant.example.com:8123"));
  EXPECT_EQ(
      0, _putenv_s("MODET_HASS_ENTITY_ID", "binary_sensor.test_binary_sensor"));
  EXPECT_EQ(0, _putenv_s("MODET_HASS_TOKEN", "test_hass_token"));
#elif __linux__
  EXPECT_EQ(
      0, setenv("MODET_HASS_URL", "https://homeassistant.example.com:8123", 1));
  EXPECT_EQ(
      0, setenv("MODET_HASS_ENTITY_ID", "binary_sensor.test_binary_sensor", 1));
  EXPECT_EQ(0, setenv("MODET_HASS_TOKEN", "test_hass_token", 1));
#endif

  static const char *argv[3] = {PROJECT_NAME, "--source-url",
                                "rtsp://feed.example.com:554"};

  const auto fromEnv = std::get<util::ProgramOptions>(
      util::ProgramOptions::ParseOptions(3, argv));
  EXPECT_TRUE(fromEnv.CanSetupHass(fromEnv.feeds.at(0)));
}