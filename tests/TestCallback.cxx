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
  EXPECT_THROW(std::invoke([entityId = std::string(GetParam())] {
                 auto binarySensor =
                     std::make_unique<callback::ThreadedHassHandler>(
                         SimServer::GetBaseUrl(), "invalid_token", entityId);
                 binarySensor->Start();
               }),
               std::runtime_error);
}

void EndLoop(void *clientData) {
  auto *wv = static_cast<EventLoopWatchVariable *>(clientData);
  wv->store(1);
}

void Kickoff(void *clientData) {
  auto *sync = static_cast<std::barrier<> *>(clientData);
  sync->arrive_and_drop();
}

class TestAsyncHassHandler : public testing::TestWithParam<std::string_view> {
protected:
  void SetUp() override {
    pSched_ = decltype(pSched_)(BasicTaskScheduler::createNew());
  }

  std::shared_ptr<TaskScheduler> pSched_;
};

TEST_P(TestAsyncHassHandler, CanPostEntityUpdate) {
  const int startApiCalls = SimServer::GetHassApiCount();
  const std::string entityId{GetParam()};
  {
    EventLoopWatchVariable wv{0};
    std::barrier sync(2);

    auto binarySensor = std::make_shared<callback::AsyncHassHandler>(
        pSched_, SimServer::GetBaseUrl(), sim_token::bearer, entityId);
    binarySensor->debounceTime = 0s;
    binarySensor->Register();

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
    pSched_ = decltype(pSched_)(BasicTaskScheduler::createNew());
    std::random_device rd;
    std::mt19937 engine(rd());
    std::uniform_int_distribution<unsigned int> dist(0x0000'0000, 0xFFFF'FFFF);
    downloadDir_ = std::filesystem::temp_directory_path() /
                   std::format("TestAsyncFileSave/{:08x}", dist(engine));
    std::filesystem::create_directories(downloadDir_);
  }
  void TearDown() override { std::filesystem::remove_all(downloadDir_); }

  std::shared_ptr<TaskScheduler> pSched_;
  std::filesystem::path downloadDir_;
  EventLoopWatchVariable wv_{0};

  static void DownloadFile(void *clientData) {
    static int counter{1};
    auto *pAsyncFileSave = static_cast<callback::AsyncFileSave *>(clientData);
    pAsyncFileSave->SaveFileAtEndpoint(std::to_string(counter++) + ".jpg");
  }

  struct TryEndLoopData {
    callback::AsyncFileSave *pAsyncFileSave{nullptr};
    EventLoopWatchVariable *pWatchVariable{nullptr};
    TaskScheduler *pSched{nullptr};
    size_t target{0};
  };

  static void TryEndLoop(void *clientData) {
    auto [pAsyncFileSave, pWv, pSched, target] =
        *static_cast<TryEndLoopData *>(clientData);
    if (pAsyncFileSave->GetPendingRequestOperations() == 0 &&
        pAsyncFileSave->GetPendingFileOperations() == 0) {
      pSched->scheduleDelayedTask(0, EndLoop, pWv);
    } else {
      pSched->scheduleDelayedTask(1000, TryEndLoop, clientData);
    }
  }
};

TEST_F(TestAsyncFileSave, CanSaveSimultaneousImages) {

  static constexpr int width{680};
  static constexpr int height{480};
  static constexpr int shapes{12};

  auto url = SimServer::GetBaseUrl();
  url.set_path("/api/getimage");
  url.set_params({{"width", std::to_string(width)},
                  {"height", std::to_string(height)},
                  {"shapes", std::to_string(shapes)}});

  auto asyncFileSave =
      std::make_shared<callback::AsyncFileSave>(pSched_, downloadDir_, url);
  asyncFileSave->debounceTime = 0s;

  static constexpr int imgLimit{20};
  asyncFileSave->SetLimitSavedFilePaths(imgLimit);

  asyncFileSave->Register();

  static constexpr int imgCount{25};
  static constexpr int interval{
      200'000}; // this interval is to handle a limitation with mongoose when it
                // receives too many requests, this might not be required in
                // practice with production servers

  for (int i{0}; i < imgCount; ++i) {
    pSched_->scheduleDelayedTask(i * interval, TestAsyncFileSave::DownloadFile,
                                 asyncFileSave.get());
  }

  TryEndLoopData tryEndLoopData{asyncFileSave.get(), &wv_, pSched_.get()};

  static constexpr auto timeout = 10'000'000us; // 10 seconds

  pSched_->scheduleDelayedTask(imgCount * interval, TryEndLoop,
                               &tryEndLoopData);
  pSched_->scheduleDelayedTask(timeout.count(), EndLoop, &wv_);

  pSched_->doEventLoop(&wv_);

  EXPECT_EQ(asyncFileSave->GetPendingRequestOperations(), 0);
  EXPECT_EQ(asyncFileSave->GetPendingFileOperations(), 0);
  EXPECT_EQ(asyncFileSave->GetSavedFilePaths().size(), imgLimit);

  std::vector<cv::Mat> readImgs;
  for (const auto &fs : asyncFileSave->GetSavedFilePaths()) {
    readImgs.push_back(cv::imread(fs.string(), cv::IMREAD_UNCHANGED));
    EXPECT_FALSE(readImgs.back().empty())
        << "Could not read downloaded image back at " << fs.string();
    EXPECT_EQ(readImgs.back().cols, width);
    EXPECT_EQ(readImgs.back().rows, height);
    EXPECT_EQ(readImgs.back().channels(), 3);
  }

  for (size_t i = 0; i < readImgs.size(); ++i) {
    for (size_t j = i + 1; j < readImgs.size(); ++j) {
      thread_local cv::Mat diff;
      cv::absdiff(readImgs[i], readImgs[j], diff);
      thread_local std::vector<cv::Mat> diffChannels;
      cv::split(diff, diffChannels);
      for (const auto &dc : diffChannels) {
        EXPECT_GT(cv::countNonZero(dc), 0)
            << "Images " << i << " and " << j
            << " are identical, this should not happen with random images";
      }
    }
  }
}

TEST_F(TestAsyncFileSave, CanSaveALargeImage) {
  static constexpr int width{3840};
  static constexpr int height{2160};
  static constexpr int shapes{1200};

  auto url = SimServer::GetBaseUrl();
  url.set_path("/api/getimage");
  url.set_params({{"width", std::to_string(width)},
                  {"height", std::to_string(height)},
                  {"shapes", std::to_string(shapes)}});

  auto asyncFileSave =
      std::make_shared<callback::AsyncFileSave>(pSched_, downloadDir_, url);
  asyncFileSave->debounceTime = 0s;

  static constexpr int imgLimit{20};
  asyncFileSave->SetLimitSavedFilePaths(imgLimit);
  asyncFileSave->Register();

  pSched_->scheduleDelayedTask(1000, TestAsyncFileSave::DownloadFile,
                               asyncFileSave.get());

  TryEndLoopData tryEndLoopData{asyncFileSave.get(), &wv_, pSched_.get()};
  pSched_->scheduleDelayedTask(1000, TryEndLoop, &tryEndLoopData);

  pSched_->scheduleDelayedTask((20'000'000us).count(), EndLoop,
                               &wv_); // timeout 10s

  pSched_->doEventLoop(&wv_);

  cv::Mat readImg;
  for (const auto &fs : asyncFileSave->GetSavedFilePaths()) {
    cv::imread(fs.string(), readImg, cv::IMREAD_UNCHANGED);
    EXPECT_FALSE(readImg.empty())
        << "Could not read downloaded image back at " << fs.string();
    EXPECT_EQ(readImg.cols, width);
    EXPECT_EQ(readImg.rows, height);
    EXPECT_EQ(readImg.channels(), 3);
  }

  EXPECT_EQ(asyncFileSave->GetPendingRequestOperations(), 0);
  EXPECT_EQ(asyncFileSave->GetPendingFileOperations(), 0);
  EXPECT_LT(asyncFileSave->GetSavedFilePaths().size(), imgLimit);
}