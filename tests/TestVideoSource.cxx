#include "WindowsWrapper.h"

#include <gtest/gtest.h>

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

TEST(Live555VideoSourceTests, Smoke) {
  video_source::Live555VideoSource live555(boost::url(""));
  EXPECT_THROW(live555.StartStream(0), std::runtime_error);
}