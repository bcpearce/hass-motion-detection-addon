#include <gtest/gtest.h>

#include "HomeAssistant/AsyncHassHandler.h"
#include "HomeAssistant/ThreadedHassHandler.h"
#include "SimServer.h"
#include "Util/CurlWrapper.h"

#include <BasicUsageEnvironment.hh>
#include <chrono>
#include <string_view>

using namespace std::chrono_literals;
using namespace std::string_view_literals;

class TestHassHandler : public testing::TestWithParam<std::string> {};

TEST_P(TestHassHandler, CanPostBinarySensorUpdate) {
  const int startApiCalls = SimServer::GetHassApiCount();
  const std::string entityId{GetParam()};
  {
    auto binarySensor = std::make_shared<home_assistant::ThreadedHassHandler>(
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
                 auto binarySensor =
                     std::make_unique<home_assistant::ThreadedHassHandler>(
                         SimServer::GetBaseUrl(), "invalid_token", entityId);
                 binarySensor->Start();
               }),
               std::runtime_error);
}

static constexpr auto binary_sensor__missing{"binary_sensor.missing"sv};
static constexpr auto binary_sensor__motion_detector{
    "binary_sensor.motion_detector"sv};
static constexpr auto sensor__motion_objects{"sensor.motion_objects"sv};

INSTANTIATE_TEST_SUITE_P(
    EntityIds, TestHassHandler,
    testing::Values(std::string(binary_sensor__missing),
                    std::string(binary_sensor__motion_detector),
                    std::string(sensor__motion_objects)));

void EndLoop(void *clientData) {
  auto *wv = std::bit_cast<EventLoopWatchVariable *>(clientData);
  wv->store(1);
}

class AsyncHassHandlerTests : public testing::Test {
protected:
  void SetUp() override { pSched_ = BasicTaskScheduler::createNew(); }
  void TearDown() override { delete pSched_; }

  TaskScheduler *pSched_{nullptr};
};

TEST_F(AsyncHassHandlerTests, CanPostBinarySensor) {
  const int startApiCalls = SimServer::GetHassApiCount();
  std::string entityId{"binary_sensor.motion_detector"sv};
  {

    auto binarySensor = std::make_unique<home_assistant::AsyncHassHandler>(
        SimServer::GetBaseUrl(), sim_token::bearer, entityId);
    binarySensor->debounceTime = 0s;

    binarySensor->Register(pSched_);

    (*binarySensor)({});
    std::vector rois = {cv::Rect(50, 50, 50, 50)};

    (*binarySensor)(rois);

    EventLoopWatchVariable wv{0};

    const auto trigger = pSched_->createEventTrigger(EndLoop);

    std::jthread watcher([&] {
      EXPECT_EQ(std::future_status::ready,
                SimServer::WaitForHassApiCount(startApiCalls + 2, 10s));
      pSched_->triggerEvent(trigger, &wv);
    });

    pSched_->doEventLoop(&wv);
  }
}