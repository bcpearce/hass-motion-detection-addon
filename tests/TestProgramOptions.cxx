#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "Util/ProgramOptions.h"

TEST(ProgramOptionsTests, Parse1) {
  std::vector<const char *> argv{"MotionDetection", "--source-url",
                                 "rtsp://feed.example.com:554"};
  const util::ProgramOptions progOpts(argv.size(), argv.data());
  EXPECT_STREQ(progOpts.url.c_str(), "rtsp://feed.example.com:554");
}

TEST(ProgramOptionsTests, ParseDetectionSize) {
  std::vector<const char *> argv{"MotionDetection", "--source-url",
                                 "rtsp://feed.example.com:554",
                                 "--detection-size", "32.045%"};
  const util::ProgramOptions progOpts(argv.size(), argv.data());
  EXPECT_THAT(progOpts.detectionSize, testing::VariantWith<double>(
                                          testing::DoubleNear(0.32045, 0.001)));
}

TEST(ProgramOptionsTests, CanSetupHass) {
  std::vector<const char *> argv{"MotionDetection", "--source-url",
                                 "rtsp://feed.example.com:554"};
  const util::ProgramOptions progOptsFalse(argv.size(), argv.data());
  EXPECT_FALSE(progOptsFalse.CanSetupHass());

  argv.push_back("--hass-url");
  argv.push_back("https://hass.example.com");
  argv.push_back("--hass-entity-id");
  argv.push_back("binary_sensor.hello");
  argv.push_back("--hass-token");
  argv.push_back("abcd");

  const util::ProgramOptions progOptsTrue(argv.size(), argv.data());
  EXPECT_TRUE(progOptsTrue.CanSetupHass());
}