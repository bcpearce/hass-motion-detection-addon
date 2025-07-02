#include <gtest/gtest.h>

#include "HomeAssistant/ThreadedHassHandler.h"
#include "SimServer.h"
#include "Util/CurlWrapper.h"

#include <chrono>
#include <string_view>

using namespace std::chrono_literals;
using namespace std::string_view_literals;

class TestHassHandler : public testing::TestWithParam<std::string_view> {};

TEST_P(TestHassHandler, CanPostBinarySensorUpdate) {
  const int startApiCalls = SimServer::GetHassApiCount();
  const std::string_view entityId{GetParam()};
  {
    auto binarySensor = home_assistant::HassHandler::Create(
        SimServer::GetBaseUrl(), sim_token::bearer, entityId);
    binarySensor->debounceTime = 0s;
    binarySensor->Start();

    (*binarySensor)({});
    std::vector rois = {cv::Rect(50, 50, 50, 50)};

    (*binarySensor)(rois);
  }
  EXPECT_GT(SimServer::GetHassApiCount(), startApiCalls);
}

TEST_P(TestHassHandler, FailsWithoutBearerToken) {
  const int startApiCalls = SimServer::GetHassApiCount();
  EXPECT_THROW(std::invoke([entityId = GetParam()] {
                 auto binarySensor = home_assistant::HassHandler::Create(
                     SimServer::GetBaseUrl(), "invalid_token", entityId);
                 binarySensor->Start();
               }),
               std::runtime_error);
}

static constexpr auto binary_sensor__missing{"binary_sensor.missing"sv};
static constexpr auto binary_sensor__motion_detector{
    "binary_sensor.motion_detector"sv};
static constexpr auto sensor__motion_objects{"sensor.motion_objects"sv};

INSTANTIATE_TEST_SUITE_P(EntityIds, TestHassHandler,
                         testing::Values(binary_sensor__missing,
                                         binary_sensor__motion_detector,
                                         sensor__motion_objects));