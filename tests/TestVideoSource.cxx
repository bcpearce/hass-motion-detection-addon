#include "WindowsWrapper.h"

#include <gtest/gtest.h>

#include <BasicUsageEnvironment.hh>

#include "VideoSource/Http.h"
#include "VideoSource/Live555.h"

#include "SimServer.h"

using namespace std::chrono_literals;

TEST(HttpVideoSourceTests, TestReceiveFrame) {
  try {
    auto url = SimServer::GetBaseUrl();
    url.set_path("/api/getimage");
    url.set_params({{"width", "640"}, {"height", "480"}});
    video_source::HttpVideoSource http(url);

    auto handler = [&](const video_source::Frame &frame) {
      EXPECT_FALSE(frame.img.empty());
      EXPECT_EQ(640, frame.img.cols);
      EXPECT_EQ(480, frame.img.rows);
      EXPECT_NO_THROW(http.StopStream());
    };

    http.Subscribe(handler);
    http.timeout = 5s;

    EXPECT_NO_THROW(http.StartStream());
  } catch (const std::exception &e) {
    FAIL() << "Exception thrown: " << e.what();
  }
}

TEST(HttpVideoSourceTests, TestReceiveLargeFrame) {
  try {
    static constexpr int width{3840};
    static constexpr int height{2160};
    static constexpr int shapes{6000};

    auto url = SimServer::GetBaseUrl();
    url.set_path("/api/getimage");
    url.set_params({{"width", std::to_string(width)},
                    {"height", std::to_string(height)},
                    {"shapes", std::to_string(shapes)}});

    video_source::HttpVideoSource http(url);
    http.delayBetweenFrames = 10s;

    std::mutex mtx;
    std::condition_variable cv;

    auto handler = [&](const video_source::Frame &frame) {
      {
        std::unique_lock lk(mtx);
        cv.notify_all();
      }
      EXPECT_FALSE(frame.img.empty());
      EXPECT_EQ(width, frame.img.cols);
      EXPECT_EQ(height, frame.img.rows);
      EXPECT_EQ(3, frame.img.channels());
      EXPECT_NO_THROW(http.StopStream());
    };

    std::jthread timeout([&] { // timeout for the test
      std::unique_lock lk(mtx);
      if (cv.wait_for(lk, 10s) == std::cv_status::timeout) {
        http.StopStream();
        FAIL() << "Timeout waiting for frame";
      }
    });

    http.Subscribe(handler);
    http.timeout = 10s; // timeout for the request

    EXPECT_NO_THROW(http.StartStream(1));
  } catch (const std::exception &e) {
    FAIL() << "Exception thrown: " << e.what();
  }
}

TEST(Live555VideoSourceTests, NoUrl) {
  auto pSched = std::shared_ptr<TaskScheduler>(BasicTaskScheduler::createNew());
  video_source::Live555VideoSource live555(pSched, boost::url(""));
  EXPECT_THROW(live555.StartStream(0), std::runtime_error);
}

TEST(Live555VideoSourceTests, BadUrl) {
  auto pSched = std::shared_ptr<TaskScheduler>(BasicTaskScheduler::createNew());
  video_source::Live555VideoSource live555(pSched,
                                           boost::url("http://localhost"));
  EXPECT_NO_THROW(live555.StartStream())
      << "Expected Stream to fail due to incorrect protocol";
}

void StopStream(void *clientData) {
  auto &wv = *static_cast<EventLoopWatchVariable *>(clientData);
  wv.store(1);
}

TEST(Live555VideoSourceTests, Smoke) {
  GTEST_SKIP() << "Test not ready yet";
  auto pSched = std::shared_ptr<TaskScheduler>(BasicTaskScheduler::createNew());
  video_source::Live555VideoSource live555(
      pSched, boost::url("rtsp://localhost:8554/h264ESVideoTest"));
  live555.StartStream();
  EventLoopWatchVariable wv;
  pSched->scheduleDelayedTask(
      std::chrono::microseconds(std::chrono::seconds(5)).count(), StopStream,
      &wv);
  pSched->doEventLoop(&wv);
}