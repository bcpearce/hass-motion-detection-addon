#include "WindowsWrapper.h"

#include <gtest/gtest.h>

#include "Util/BufferOperations.h"
#include "Util/CurlMultiWrapper.h"
#include "Util/CurlWrapper.h"

#include "SimServer.h"

TEST(CurlWrapperTests, Smoke) {
  util::CurlWrapper wCurl;

  curl_off_t uploadData{0};
  wCurl(curl_easy_getinfo, CURLINFO_CONTENT_LENGTH_UPLOAD_T, &uploadData);
  EXPECT_EQ(uploadData, -1);
  EXPECT_NO_THROW(curl_easy_reset(&wCurl));
}

TEST(CurlMultiWrapperTests, Smoke) {
  util::CurlMultiWrapper wCurl;
  CURL **handles = curl_multi_get_handles(wCurl.pCurl_);
  EXPECT_EQ(handles[0], nullptr);
  curl_free(handles);
}

TEST(CurlWrapperTests, CanMakeGetRequest) {

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