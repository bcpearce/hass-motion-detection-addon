#include <iostream>

#include <array>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <ranges>
#include <vector>

#include <curl/curl.h>
#include <openssl/ssl.h>

size_t FillCvMatCallback(void *contents, size_t sz, size_t nmemb,
                         void *pUserData) {
  std::cout << "Callback " << sz << "x" << nmemb << "\n";
  if (pUserData) {
    std::vector<uint8_t> &buf = *static_cast<std::vector<uint8_t> *>(pUserData);
    size_t realsize = sz * nmemb;
    buf.reserve(buf.size() + realsize);
    std::ranges::copy(std::span<uint8_t>((uint8_t *)contents, realsize),
                      std::back_inserter(buf));
    return realsize;
  }
  return 0;
}

static constexpr auto url{"https://webcams.nyctmc.org/api/cameras/"
                          "a454105b-8f59-4d96-a656-9a2d29f1353d/image"};

int main(int argc, char **argv) {
  std::vector<uint8_t> buf;
  buf.reserve(CURL_MAX_WRITE_SIZE);

  cv::Mat img, bg, canvas, grayImg, grayBg, grayDiff, bgrDat, thresh, morph;

  for (int key = ' '; key == ' ' || key == -1; key = cv::waitKeyEx(2000)) {
    if (key != -1) {
      std::cout << "key pressed: " << key << "\n";
    }
    buf.clear();
    CURL *hCurl = curl_easy_init();
    if (hCurl) {
      const auto secsFromEpoch = std::chrono::system_clock::to_time_t(
          std::chrono::system_clock::now());
      std::cout << "Get from " << url << secsFromEpoch << "\n";
      curl_easy_setopt(hCurl, CURLOPT_URL, url);
      curl_easy_setopt(hCurl, CURLOPT_WRITEFUNCTION, FillCvMatCallback);
      curl_easy_setopt(hCurl, CURLOPT_WRITEDATA, &buf);
      CURLcode res = curl_easy_perform(hCurl);
      if (res == CURLE_OK) {
        char *ct{nullptr};
        res = curl_easy_getinfo(hCurl, CURLINFO_CONTENT_TYPE, &ct);
        if ((res == CURLE_OK) && ct) {
          std::cout << "Content type: " << ct << "\n";
        }
        cv::imdecode(buf, cv::IMREAD_COLOR, &img);

        if (bg.empty()) {
          img.copyTo(bg);
        } else {
          cv::addWeighted(bg, 0.8, img, 0.2, 0, bg);
        }

        cv::cvtColor(img, grayImg, cv::COLOR_BGR2GRAY);
        cv::cvtColor(bg, grayBg, cv::COLOR_BGR2GRAY);
        cv::absdiff(grayImg, grayBg, grayDiff);

        cv::threshold(grayDiff, thresh, 60, 255, cv::THRESH_BINARY);
        cv::morphologyEx(
            thresh, morph, cv::MORPH_DILATE,
            cv::getStructuringElement(cv::MORPH_OPEN, cv::Size(5, 5)));

        cv::cvtColor(morph, bgrDat, cv::COLOR_GRAY2BGR);

        std::array ia{img, bg, bgrDat};
        cv::hconcat(ia, canvas);

        cv::imshow("WebImage", canvas);
      }
    }
    curl_easy_cleanup(hCurl);
  }

  return 0;
}