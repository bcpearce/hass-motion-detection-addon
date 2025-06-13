#include "WindowsWrapper.h"

#include <gtest/gtest.h>

#include "VideoSource/Http.h"
#include "VideoSource/Live555.h"

TEST(HttpVideoSourceTests, Smoke) {
  video_source::HttpVideoSource http("http://example.com/example");
  EXPECT_NO_THROW(http.InitStream());
  EXPECT_NO_THROW(http.StopStream());
}

TEST(Live555VideoSourceTests, Smoke) {
  video_source::Live555VideoSource live555("");
  EXPECT_THROW(live555.InitStream(), std::runtime_error);
}