#include <gtest/gtest.h>

#include "HomeAssistant/AsyncHassHandler.h"
#include "HomeAssistant/SyncHassHandler.h"
#include "HomeAssistant/ThreadedHassHandler.h"
#include "SimServer.h"
#include "Util/CurlWrapper.h"

#include <BasicUsageEnvironment.hh>
#include <chrono>
#include <string_view>

using namespace std::chrono_literals;
using namespace std::string_view_literals;

class TestThreadedHassHandler
    : public testing::TestWithParam<std::string_view> {};

TEST_P(TestThreadedHassHandler, CanPostEntityUpdate) {
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

TEST_P(TestThreadedHassHandler, FailsWithoutBearerToken) {
  const int startApiCalls = SimServer::GetHassApiCount();
  EXPECT_THROW(std::invoke([entityId = std::string(GetParam())] {
                 auto binarySensor =
                     std::make_unique<home_assistant::ThreadedHassHandler>(
                         SimServer::GetBaseUrl(), "invalid_token", entityId);
                 binarySensor->Start();
               }),
               std::runtime_error);
}

void EndLoop(void *clientData) {
  auto *wv = std::bit_cast<EventLoopWatchVariable *>(clientData);
  wv->store(1);
}

class TestAsyncHassHandler : public testing::TestWithParam<std::string_view> {
protected:
  void SetUp() override { pSched_ = BasicTaskScheduler::createNew(); }
  void TearDown() override { delete pSched_; }

  TaskScheduler *pSched_{nullptr};
};

TEST_P(TestAsyncHassHandler, CanPostEntityUpdate) {
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

class TestSyncHassHandler : public testing::TestWithParam<std::string_view> {};

TEST_P(TestSyncHassHandler, CanPostEntityUpdate) {
  const int startApiCalls = SimServer::GetHassApiCount();
  const std::string entityId{GetParam()};

  home_assistant::SyncHassHandler syncHandler(SimServer::GetBaseUrl(),
                                              sim_token::bearer, entityId);
  syncHandler({});
  // Expect 2 calls, one to get the initial data, then a second to update it
  // with our changes
  std::jthread watcher([&] {
    EXPECT_EQ(std::future_status::ready,
              SimServer::WaitForHassApiCount(startApiCalls + 2, 10s));
  });
}

static constexpr auto binary_sensor__missing{"binary_sensor.missing"sv};
static constexpr auto binary_sensor__motion_detector{
    "binary_sensor.motion_detector"sv};
static constexpr auto sensor__motion_objects{"sensor.motion_objects"sv};

static const auto hassEntityIdValues =
    testing::Values(binary_sensor__missing, binary_sensor__motion_detector,
                    sensor__motion_objects);

INSTANTIATE_TEST_SUITE_P(EntityIds, TestThreadedHassHandler,
                         hassEntityIdValues);
INSTANTIATE_TEST_SUITE_P(EntityIds, TestAsyncHassHandler, hassEntityIdValues);
INSTANTIATE_TEST_SUITE_P(EntityIds, TestSyncHassHandler, hassEntityIdValues);
