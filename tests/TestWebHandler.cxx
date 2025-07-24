#include "WindowsWrapper.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_set>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#define JSON_USE_IMPLICIT_CONVERSIONS 0
#include <nlohmann/json.hpp>

#include "Gui/Payload.h"
#include "Gui/WebHandler.h"
#include "Util/BufferOperations.h"
#include "Util/CurlWrapper.h"

using namespace std::string_literals;
using namespace std::string_view_literals;

using json = nlohmann::json;

class WebHandlerTests : public testing::TestWithParam<int> {
protected:
  void SetUp() override {
    pWh_ = std::make_unique<gui::WebHandler>(32836);
    pWh_->Start();
  }

  void TearDown() override { pWh_->Stop(); }

  std::string GetServerUrl() const { return "http://localhost:32836"s; }

  std::unique_ptr<gui::WebHandler> pWh_;
};

TEST_P(WebHandlerTests, CanSetImage) {

  gui::Payload data;

  const int matType{GetParam()};

  using sc = std::chrono::steady_clock;

  data.frame = {.id = 0,
                .img = cv::Mat::zeros(400, 400, matType),
                .timeStamp = sc::time_point::min()};
  data.detail = cv::Mat::zeros(400, 400, matType);
  data.fps = 30.0;

  EXPECT_NO_THROW((*pWh_)(data));
}

TEST_F(WebHandlerTests, CanAccessPackedFilesystem) {

  // read the packed index.html to compare later
  const auto indexPath =
      std::filesystem::path(__FILE__).parent_path().parent_path() / "src" /
      "Gui" / "public" / "index.html";

  std::ifstream fs(indexPath, std::ios::binary | std::ios::ate);
  std::streamsize sz = fs.tellg();
  std::vector<char> expected(sz);
  fs.seekg(0, std::ios::beg);
  ASSERT_TRUE(fs.read(expected.data(), sz))
      << "Failed to read file at " << indexPath.string();

  util::CurlWrapper wCurl;
  std::vector<char> buf;
  EXPECT_NO_THROW(std::invoke([&] {
    wCurl(curl_easy_setopt, CURLOPT_URL, GetServerUrl().c_str());
    wCurl(curl_easy_setopt, CURLOPT_WRITEDATA, &buf);
    wCurl(curl_easy_setopt, CURLOPT_WRITEFUNCTION, util::FillBufferCallback);
    wCurl(curl_easy_perform);
  }));

  EXPECT_EQ(std::string_view(buf), std::string_view(expected));
}

TEST_F(WebHandlerTests, LoadAndReadBackFeedIds) {
  (*pWh_)({.feedId = "feed1"sv});
  (*pWh_)({.feedId = "feed2"sv});
  (*pWh_)({.feedId = "feed3"sv});

  util::CurlWrapper wCurl;
  std::vector<char> buf;
  EXPECT_NO_THROW(std::invoke([&] {
    const auto url = GetServerUrl() + "/media/feeds"s;
    wCurl(curl_easy_setopt, CURLOPT_URL, url.c_str());
    wCurl(curl_easy_setopt, CURLOPT_WRITEDATA, &buf);
    wCurl(curl_easy_setopt, CURLOPT_WRITEFUNCTION, util::FillBufferCallback);
    wCurl(curl_easy_perform);
  }));

  json res = json::parse(buf);

  EXPECT_THAT(res, testing::Contains("feed1"));
  EXPECT_THAT(res, testing::Contains("feed2"));
  EXPECT_THAT(res, testing::Contains("feed3"));
}

INSTANTIATE_TEST_SUITE_P(ImageTypes, WebHandlerTests,
                         testing::Values(CV_8UC1, CV_8UC3, CV_8UC4));