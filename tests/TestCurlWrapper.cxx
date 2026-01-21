#include "WindowsWrapper.h"

#include <gtest/gtest.h>

#include "Util/BufferOperations.h"
#include "Util/CurlMultiWrapper.h"
#include "Util/CurlWrapper.h"

#include "SimServer.h"

TEST(CurlWrapperTests, Smoke) {
  util::CurlWrapper wCurl;
  util::CurlWrapper otherWCurl = std::move(wCurl);
  util::CurlWrapper yetAnotherWCurl(std::move(otherWCurl));
  EXPECT_EQ(nullptr, &wCurl);
  EXPECT_EQ(nullptr, &otherWCurl);

  curl_off_t uploadData{0};
  yetAnotherWCurl(curl_easy_getinfo, CURLINFO_CONTENT_LENGTH_UPLOAD_T,
                  &uploadData);
  EXPECT_EQ(uploadData, -1);
  EXPECT_NO_THROW(curl_easy_reset(&yetAnotherWCurl));
}

TEST(CurlMultiWrapperTests, Smoke) {
  util::CurlMultiWrapper wCurlMulti;
  util::CurlMultiWrapper otherWCurlMulti = std::move(wCurlMulti);
  util::CurlMultiWrapper yetAnotherWCurlMulti(std::move(otherWCurlMulti));
  EXPECT_EQ(nullptr, wCurlMulti.pCurl_);
  EXPECT_EQ(nullptr, otherWCurlMulti.pCurl_);

  CURL **handles = curl_multi_get_handles(yetAnotherWCurlMulti.pCurl_);
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