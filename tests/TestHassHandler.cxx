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

class TestHassHandler : public testing::TestWithParam<std::string_view> {};

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

    std::jthread watcher([&] {
      EXPECT_EQ(std::future_status::ready,
                SimServer::WaitForHassApiCount(startApiCalls + 2, 10s));
    });
  }
}

TEST_P(TestHassHandler, FailsWithoutBearerToken) {
  const int startApiCalls = SimServer::GetHassApiCount();
  EXPECT_THROW(std::invoke([entityId = std::string(GetParam())] {
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

INSTANTIATE_TEST_SUITE_P(EntityIds, TestHassHandler,
                         testing::Values(binary_sensor__missing,
                                         binary_sensor__motion_detector,
                                         sensor__motion_objects));

void EndLoop(void *clientData) {
  auto *wv = std::bit_cast<EventLoopWatchVariable *>(clientData);
  wv->store(1);
}

class AsyncTestHassHandler : public testing::TestWithParam<std::string_view> {
protected:
  void SetUp() override { pSched_ = BasicTaskScheduler::createNew(); }
  void TearDown() override { delete pSched_; }

  TaskScheduler *pSched_{nullptr};
};

TEST_P(AsyncTestHassHandler, CanPostBinarySensorUpdate) {
  const int startApiCalls = SimServer::GetHassApiCount();
  const std::string entityId{GetParam()};
  {

    auto binarySensor = std::make_shared<home_assistant::AsyncHassHandler>(
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
      std::this_thread::sleep_for(1s);
      pSched_->triggerEvent(trigger, &wv);
    });

    pSched_->doEventLoop(&wv);
  }
}

INSTANTIATE_TEST_SUITE_P(EntityIds, AsyncTestHassHandler,
                         testing::Values(binary_sensor__missing,
                                         binary_sensor__motion_detector,
                                         sensor__motion_objects));