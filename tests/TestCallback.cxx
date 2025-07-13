#include <gtest/gtest.h>

#include "Callback/AsyncFileSave.h"
#include "Callback/AsyncHassHandler.h"
#include "Callback/SyncHassHandler.h"
#include "Callback/ThreadedHassHandler.h"
#include "SimServer.h"
#include "Util/CurlWrapper.h"

#include <BasicUsageEnvironment.hh>
#include <opencv2/imgcodecs.hpp>

#include <barrier>
#include <bit>
#include <chrono>
#include <filesystem>
#include <format>
#include <list>
#include <random>
#include <string_view>

using namespace std::chrono_literals;
using namespace std::string_view_literals;

class TestThreadedHassHandler
    : public testing::TestWithParam<std::string_view> {};

TEST_P(TestThreadedHassHandler, CanPostEntityUpdate) {
  const int startApiCalls = SimServer::GetHassApiCount();
  const std::string entityId{GetParam()};
  {
    auto binarySensor = std::make_shared<callback::ThreadedHassHandler>(
        SimServer::GetBaseUrl(), sim_token::bearer, entityId);
    binarySensor->debounceTime = 0s;
    binarySensor->Start();

    (*binarySensor)({});
    std::vector rois = {cv::Rect(50, 50, 50, 50)};

    (*binarySensor)(rois);

    std::jthread watcher([&] {
      EXPECT_EQ(startApiCalls + 2,
                SimServer::WaitForHassApiCount(startApiCalls + 2, 10s));
    });
  }
}

TEST_P(TestThreadedHassHandler, FailsWithoutBearerToken) {
  const int startApiCalls = SimServer::GetHassApiCount();
  EXPECT_THROW(std::invoke([entityId = std::string(GetParam())] {
                 auto binarySensor =
                     std::make_unique<callback::ThreadedHassHandler>(
                         SimServer::GetBaseUrl(), "invalid_token", entityId);
                 binarySensor->Start();
               }),
               std::runtime_error);
}

void EndLoop(void *clientData) {
  auto *wv = std::bit_cast<EventLoopWatchVariable *>(clientData);
  wv->store(1);
}

void Kickoff(void *clientData) {
  auto *sync = std::bit_cast<std::barrier<> *>(clientData);
  sync->arrive_and_drop();
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
    EventLoopWatchVariable wv{0};
    std::barrier sync(2);

    auto binarySensor = std::make_shared<callback::AsyncHassHandler>(
        SimServer::GetBaseUrl(), sim_token::bearer, entityId);
    binarySensor->debounceTime = 0s;

    binarySensor->Register(pSched_);

    std::vector rois = {cv::Rect(50, 50, 50, 50)};

    (*binarySensor)(rois);

    const auto trigger = pSched_->createEventTrigger(EndLoop);
    pSched_->scheduleDelayedTask(50'000, Kickoff, &sync);

    std::jthread driver([&] {
      sync.arrive_and_wait();

      EXPECT_EQ(startApiCalls + 1,
                SimServer::WaitForHassApiCount(startApiCalls + 1, 10s));
      pSched_->triggerEvent(trigger, &wv);
    });

    pSched_->doEventLoop(&wv);
  }
}

class TestSyncHassHandler : public testing::TestWithParam<std::string_view> {};

TEST_P(TestSyncHassHandler, CanPostEntityUpdate) {
  const int startApiCalls = SimServer::GetHassApiCount();
  const std::string entityId{GetParam()};

  callback::SyncHassHandler syncHandler(SimServer::GetBaseUrl(),
                                        sim_token::bearer, entityId);
  syncHandler({});
  // Expect 2 calls, one to get the initial data, then a second to update it
  // with our changes
  EXPECT_EQ(startApiCalls + 2,
            SimServer::WaitForHassApiCount(startApiCalls + 2, 10s));
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

class TestAsyncFileSave : public testing::Test {
protected:
  void SetUp() override {
    pSched_ = BasicTaskScheduler::createNew();
    std::random_device rd;
    std::mt19937 engine(rd());
    std::uniform_int_distribution<unsigned int> dist(0x0000'0000, 0xFFFF'FFFF);
    downloadDir_ = std::filesystem::temp_directory_path() /
                   std::format("TestAsyncFileSave-{:08x}", dist(engine));
    std::filesystem::create_directories(downloadDir_);
  }
  void TearDown() override {
    std::filesystem::remove_all(downloadDir_);
    delete pSched_;
  }

  TaskScheduler *pSched_{nullptr};
  std::filesystem::path downloadDir_;

  struct ClientData {
    callback::AsyncFileSave *pAsyncFileSave{nullptr};
    std::filesystem::path *pFilesystemPath{nullptr};
  };

  static void DownloadFile(void *clientData) {
    auto [pAsyncFileSave, pFilesystemPath] =
        *std::bit_cast<ClientData *>(clientData);
    pAsyncFileSave->SaveFileAtEndpoint(*pFilesystemPath);
  }
};

TEST_F(TestAsyncFileSave, CanSaveAnImage) {
  std::barrier sync(2);
  auto url = SimServer::GetBaseUrl();
  url.set_path("/api/getimage");
  url.set_params({{"width", "640"}, {"height", "480"}});

  auto asyncFileSave = std::make_shared<callback::AsyncFileSave>(url);
  asyncFileSave->Register(pSched_);

  const auto trigger = pSched_->createEventTrigger(EndLoop);

  static constexpr int imgCount{25};

  std::list<std::filesystem::path> imgPaths;
  std::list<ClientData> clientDatas;
  for (auto &&fs : std::views::iota(1, imgCount + 1) |
                       std::views::transform([&](const int i) {
                         return downloadDir_ / std::format("{}.jpg", i);
                       })) {
    imgPaths.push_back(std::move(fs));
    clientDatas.push_back({.pAsyncFileSave = asyncFileSave.get(),
                           .pFilesystemPath = &imgPaths.back()});
    pSched_->scheduleDelayedTask(0, TestAsyncFileSave::DownloadFile,
                                 &clientDatas.back());
  }

  EventLoopWatchVariable wv{0};
  pSched_->scheduleDelayedTask((5'000'000us).count(), EndLoop, &wv);

  pSched_->doEventLoop(&wv);

  for (const auto &fs : imgPaths) {
    EXPECT_FALSE(cv::imread(fs.string(), cv::IMREAD_UNCHANGED).empty())
        << "Could not read downloaded image back at " << fs.string();
  }
}