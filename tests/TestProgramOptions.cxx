#include "WindowsWrapper.h"

#ifdef __linux__
#include <stdlib.h>
#endif

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "Util/ProgramOptions.h"

using namespace std::string_literals;
using namespace std::string_view_literals;
using namespace std::chrono_literals;

TEST(ProgramOptionsTests, Parse1) {
  const auto config =
      (std::filesystem::path(__FILE__).parent_path() / "res" / "Parse1.json")
          .string();
  std::vector<const char *> argv{PROJECT_NAME, "-c", config.c_str()};
  const util::ProgramOptions progOpts = std::get<util::ProgramOptions>(
      util::ProgramOptions::ParseOptions(argv.size(), argv.data()));
  EXPECT_EQ(std::string_view(progOpts.feeds.at("feed_1").sourceUrl.c_str()),
            "rtsp://feed.example.com:554"sv);
  EXPECT_THAT(
      progOpts.feeds.at("feed_1").detectionSize,
      testing::VariantWith<double>(testing::DoubleNear(0.32045, 0.001)));
}

TEST(ProgramOptionsTests, ParseMultiple) {
  const auto config = (std::filesystem::path(__FILE__).parent_path() / "res" /
                       "ParseMultiple.json")
                          .string();
  std::vector<const char *> argv{PROJECT_NAME, "-c", config.c_str()};
  const util::ProgramOptions progOpts = std::get<util::ProgramOptions>(
      util::ProgramOptions::ParseOptions(argv.size(), argv.data()));

  // FEED 1
  EXPECT_EQ(progOpts.feeds.at("feed_1").sourceUrl.c_str(),
            "rtsp://feed_1.example.com:554"sv);
  EXPECT_THAT(
      progOpts.feeds.at("feed_1").detectionSize,
      testing::VariantWith<double>(testing::DoubleNear(0.32045, 0.001)));

  // FEED 2
  EXPECT_EQ(progOpts.feeds.at("feed_2").sourceUrl.c_str(),
            "rtsp://feed_2.example.com:554"sv);
  EXPECT_THAT(progOpts.feeds.at("feed_2").detectionSize,
              testing::VariantWith<int>(1500));
  EXPECT_THAT(progOpts.feeds.at("feed_2").detectionDebounce, 30s);
  EXPECT_EQ(progOpts.feeds.at("feed_2").hassEntityId, "binary_sensor.feed_2"sv);
  EXPECT_EQ(progOpts.feeds.at("feed_2").hassFriendlyName, "Feed 2"sv);
  EXPECT_EQ(progOpts.feeds.at("feed_2").sourcePassword, "a_fine_word"sv);
  EXPECT_EQ(progOpts.feeds.at("feed_2").sourceUsername, "username"sv);
}

TEST(ProgramOptionsTests, CanSetupHass) {
  const auto config =
      (std::filesystem::path(__FILE__).parent_path() / "res" / "Parse1.json")
          .string();

  static std::vector<const char *> argvFalse = {PROJECT_NAME, "-c",
                                                config.c_str()};
  const auto progOptsFalse = std::get<util::ProgramOptions>(
      util::ProgramOptions::ParseOptions(argvFalse.size(), argvFalse.data()));
  EXPECT_FALSE(progOptsFalse.CanSetupHass(progOptsFalse.feeds.at("feed_1")))
      << "Options " << argvFalse[0] << " " << argvFalse[1] << " "
      << argvFalse[2] << " will set up Hass endpoint, but should not";

  const auto configHass = (std::filesystem::path(__FILE__).parent_path() /
                           "res" / "CanSetupHass.json")
                              .string();
  static std::vector<const char *> argvTrue = {
      PROJECT_NAME, "--hass-url", "https://hass.example.com", "--hass-token",
      "abcd",       "-c",         configHass.c_str()};

  const auto progOptsTrue = std::get<util::ProgramOptions>(
      util::ProgramOptions::ParseOptions(argvTrue.size(), argvTrue.data()));
  EXPECT_TRUE(progOptsTrue.CanSetupHass(progOptsTrue.feeds.at("feed_1")));
}

TEST(ProgramOptionsTests, CanSetupHassWithEnvironmentVariables) {
#if _WIN32
  EXPECT_EQ(
      0, _putenv_s("MODET_HASS_URL", "https://homeassistant.example.com:8123"));
  EXPECT_EQ(0, _putenv_s("MODET_HASS_TOKEN", "test_hass_token"));
#elif __linux__
  EXPECT_EQ(
      0, setenv("MODET_HASS_URL", "https://homeassistant.example.com:8123", 1));
  EXPECT_EQ(0, setenv("MODET_HASS_TOKEN", "test_hass_token", 1));
#endif
  const auto config = (std::filesystem::path(__FILE__).parent_path() / "res" /
                       "CanSetupHass.json")
                          .string();
  static std::vector<const char *> argv = {PROJECT_NAME, "-c", config.c_str()};

  const auto fromEnv = std::get<util::ProgramOptions>(
      util::ProgramOptions::ParseOptions(argv.size(), argv.data()));
  EXPECT_TRUE(fromEnv.CanSetupHass(fromEnv.feeds.at("feed_1")));
}

class ProgramOptionsInfoTests : public testing::TestWithParam<const char *> {};

TEST_P(ProgramOptionsInfoTests, HelpAndVersion) {
  static std::vector<const char *> argv = {PROJECT_NAME, GetParam()};
  const auto progOpts =
      util::ProgramOptions::ParseOptions(argv.size(), argv.data());

  EXPECT_TRUE(std::holds_alternative<std::string>(progOpts));
}

INSTANTIATE_TEST_SUITE_P(ProgramOptionsInfoTests, ProgramOptionsInfoTests,
                         testing::Values("--help", "--version", "-h", "-v"));