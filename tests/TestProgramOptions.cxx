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
  static const char *argvFalse[3] = {PROJECT_NAME, "--source-url",
                                     "rtsp://feed.example.com:554"};
  const util::ProgramOptions progOptsFalse(3, argvFalse);
  EXPECT_FALSE(progOptsFalse.CanSetupHass())
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

  const util::ProgramOptions progOptsTrue(9, argvTrue);
  EXPECT_TRUE(progOptsTrue.CanSetupHass());
}