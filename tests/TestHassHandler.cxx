#include <gtest/gtest.h>

#include "HomeAssistant/HassHandler.h"
#include "SimServer.h"
#include "Util/CurlWrapper.h"

#include <chrono>

using namespace std::chrono_literals;

TEST(TestHassHandler, CanPostBinarySensorUpdate) {
  const int startApiCalls = SimServer::GetHassApiCount();
  {
    auto binarySensor = home_assistant::HassHandler::Create(
        SimServer::GetBaseUrl(), sim_token::bearer,
        "binary_sensor.motion_detected");
    binarySensor->debounceTime = 0s;

    (*binarySensor)({});
    std::vector rois = {cv::Rect(50, 50, 50, 50)};

    (*binarySensor)(rois);
  }
  EXPECT_GT(SimServer::GetHassApiCount(), startApiCalls);
}

TEST(TestHassHandler, FailsWithoutBearerToken) {
  const int startApiCalls = SimServer::GetHassApiCount();
  EXPECT_THROW(std::invoke([] {
                 std::ignore = home_assistant::HassHandler::Create(
                     SimServer::GetBaseUrl(), "invalid_token",
                     "binary_sensor.motion_detected");
               }),
               std::runtime_error);
}