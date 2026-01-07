#include <gtest/gtest.h>

#include "Detector/MotionDetector.h"

template <typename T> class MotionDetectorTests : public ::testing::Test {};

using MotionDetectorTestsTypes = ::testing::Types<detector::BasicMotionDetector,
                                                  detector::MOGMotionDetector>;

TYPED_TEST_SUITE(MotionDetectorTests, MotionDetectorTestsTypes);

TYPED_TEST(MotionDetectorTests, TestObjectDetection) {

  for (const auto imgType : {CV_8UC1, CV_8UC3, CV_8UC4}) {
    cv::Mat bgFrame = cv::Mat::zeros(480, 640, imgType);
    cv::Mat fgFrame = cv::Mat::zeros(480, 640, imgType);

    cv::Rect fgObject(100, 100, 200, 200);
    cv::rectangle(fgFrame, fgObject, cv::Scalar(255, 255, 255), -1);

    TypeParam motionDetector({});

    using sc = std::chrono::steady_clock;

    for (size_t i = 0; i < 100; ++i) {
      motionDetector.FeedFrame(
          video_source::Frame{.id = i, .img = bgFrame, .timeStamp = sc::now()});
    }
    EXPECT_EQ(0, motionDetector.GetRois().size());

    motionDetector.FeedFrame(
        video_source::Frame{.id = 101, .img = fgFrame, .timeStamp = sc::now()});
    ASSERT_EQ(1, motionDetector.GetRois().size());

    const auto &roi = motionDetector.GetRois()[0];
    std::vector<cv::Point2i> polygon = {
        roi.tl(), roi.tl() + cv::Point2i(roi.width, 0), roi.br(),
        roi.tl() + cv::Point2i(0, roi.height)};

    // Cannot check these if the ROIs are empty
    EXPECT_GT(cv::pointPolygonTest(polygon, {150, 150}, false), 0.0)
        << "Expected point {150, 150} to be inside the detected ROI";
    EXPECT_LT(cv::pointPolygonTest(polygon, {400, 400}, false), 0.0)
        << "Expected point {300, 300} to be outside the detected ROI";

    EXPECT_NO_THROW([&] { motionDetector.ResetModel(); });
  }
}