#include "WindowsWrapper.h"

#include <gtest/gtest.h>

#include "Util/BufferOperations.h"
#include "Util/CurlWrapper.h"

#include "SimServer.h"

TEST(CurlWrapperTests, Smoke) {
  util::CurlWrapper wCurl;
  EXPECT_NO_THROW(curl_easy_reset(&wCurl));
}

TEST(CurlWrapperTests, CanMakeGetRquest) {

  util::CurlWrapper wCurl;

  boost::url url = SimServer::GetBaseUrl();
  url.set_path("/api/hello");
  wCurl(curl_easy_setopt, CURLOPT_URL, url.c_str());

  std::vector<char> buffer;

  wCurl(curl_easy_setopt, CURLOPT_TIMEOUT, 5L);
  wCurl(curl_easy_setopt, CURLOPT_WRITEFUNCTION, util::FillBufferCallback);
  wCurl(curl_easy_setopt, CURLOPT_WRITEDATA, &buffer);

  ASSERT_NO_THROW(wCurl(curl_easy_perform));

  EXPECT_EQ(std::string_view(buffer.data(), buffer.size()), "Hello There");
}