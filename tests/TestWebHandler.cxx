#include "WindowsWrapper.h"

#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

#include "Gui/Payload.h"
#include "Gui/WebHandler.h"
#include "Util/BufferOperations.h"
#include "Util/CurlWrapper.h"

class WebHandlerTests : public testing::TestWithParam<int> {};

TEST_P(WebHandlerTests, CanSetImage) {

  gui::WebHandler wh(32835);
  wh.Start();

  gui::Payload data;

  const int matType{GetParam()};

  using sc = std::chrono::steady_clock;

  data.frame = {.id = 0,
                .img = cv::Mat::zeros(400, 400, matType),
                .timeStamp = sc::time_point::min()};
  data.detail = cv::Mat::zeros(400, 400, matType);
  data.fps = 30.0;

  EXPECT_NO_THROW(wh(data));
  wh.Stop();
}

INSTANTIATE_TEST_SUITE_P(ImageTypes, WebHandlerTests,
                         testing::Values(CV_8UC1, CV_8UC3, CV_8UC4));

TEST(FilesystemTest, CanAccessPackedFilesystem) {
  gui::WebHandler wh(32836);
  wh.Start();

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
    wCurl(curl_easy_setopt, CURLOPT_URL, "http://localhost:32836/");
    wCurl(curl_easy_setopt, CURLOPT_WRITEDATA, &buf);
    wCurl(curl_easy_setopt, CURLOPT_WRITEFUNCTION, util::FillBufferCallback);
    wCurl(curl_easy_perform);
  }));

  EXPECT_EQ(std::string_view(buf), std::string_view(expected));

  wh.Stop();
}