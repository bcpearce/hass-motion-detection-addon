#include "WindowsWrapper.h"

#include <gtest/gtest.h>

#include "Util/CurlWrapper.h"

TEST(CurlWrapperTests, Smoke) {
  util::CurlWrapper wCurl;
  EXPECT_NO_THROW(curl_easy_reset(&wCurl));
}