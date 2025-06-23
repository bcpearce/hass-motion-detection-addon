#include "WindowsWrapper.h"

#include <gtest/gtest.h>

#include "Gui/Payload.h"
#include "Gui/WebHandler.h"

class WebHandlerTests : public testing::TestWithParam<int> {};

TEST_P(WebHandlerTests, CanSetImage) {

  gui::WebHandler wh(32835);
  wh.Start();

  gui::Payload data;

  const int matType{GetParam()};

  using sc = std::chrono::steady_clock;

  data.frame = {.id{0},
                .img = cv::Mat::zeros(400, 400, matType),
                .timeStamp = sc::time_point::min()};
  data.detail = cv::Mat::zeros(400, 400, matType);
  data.fps = 30.0;

  EXPECT_NO_THROW(wh(data));
  wh.Stop();
}

INSTANTIATE_TEST_SUITE_P(ImageTypes, WebHandlerTests,
                         testing::Values(CV_8UC1, CV_8UC3, CV_8UC4));