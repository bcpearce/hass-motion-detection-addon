#include "Logger.h"

#include "VideoSource/Http.h"

#include <chrono>
#include <exception>
#include <format>
#include <ranges>
#include <span>

#include <opencv2/imgcodecs.hpp>

#include "Util/BufferOperations.h"
#include "Util/CurlWrapper.h"

namespace video_source {

HttpVideoSource::HttpVideoSource(const boost::url &url,
                                 const std::string &token)
    : url_{url} {
  wCurl_(curl_easy_setopt, CURLOPT_URL, url_.c_str());
  wCurl_(curl_easy_setopt, CURLOPT_WRITEFUNCTION, util::FillBufferCallback);
  wCurl_(curl_easy_setopt, CURLOPT_WRITEDATA, &buf_);

  if (!token.empty()) {
    wCurl_(curl_easy_setopt, CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
    wCurl_(curl_easy_setopt, CURLOPT_XOAUTH2_BEARER, token.c_str());
  }
}

HttpVideoSource::HttpVideoSource(const boost::url &url,
                                 const std::string &username,
                                 const std::string &password)
    : HttpVideoSource(url) {
  wCurl_(curl_easy_setopt, CURLOPT_HTTPAUTH, CURLAUTH_DIGEST);
  wCurl_(curl_easy_setopt, CURLOPT_USERNAME, username.c_str());
  wCurl_(curl_easy_setopt, CURLOPT_PASSWORD, password.c_str());
}

void HttpVideoSource::InitStream() {
  isActive_ = true;
  eventLoopThread_ =
      std::jthread([this, url = url_](std::stop_token stopToken) {
#ifdef _WIN32
        SetThreadDescription(GetCurrentThread(), L"HTTP Stream Thread");
#endif
        while (!stopToken.stop_requested()) {
          try {
            GetNextFrame();
          } catch (const std::exception &e) {
            LOGGER->error("Error getting frame from {}: {}", url, e.what());
          }
          std::this_thread::sleep_for(delayBetweenFrames);
        }
        isActive_ = false;
      });
}

void HttpVideoSource::StopStream() {
  eventLoopThread_.request_stop();
  eventLoopThread_ = {};
}

Frame HttpVideoSource::GetNextFrame() {
  if (wCurl_) {
    buf_.clear();

    wCurl_(curl_easy_setopt, CURLOPT_TIMEOUT, timeout.count());
    const auto res = wCurl_(curl_easy_perform);

    if (res == CURLE_OK) {
      int code{0};
      wCurl_(curl_easy_getinfo, CURLINFO_RESPONSE_CODE, &code);

      if (code == 200) {
        // Good case, expect an image
        auto frame = GetCurrentFrame();
        cv::imdecode(buf_, cv::IMREAD_COLOR, &frame.img);
        ++frame.id;
        frame.timeStamp = std::chrono::steady_clock::now();
        VideoSource::SetFrame(frame);
        return frame;
      } else {
        // Bad case, expect a string
        std::string_view msg(buf_.data(), buf_.size());
        throw std::runtime_error(std::format("http error: ({}) {}", code, msg));
      }
    } else {
      const std::string_view errMsg(errBuf_.data(), CURL_ERROR_SIZE);
      throw std::runtime_error(
          std::format("libcurl: ({}) {}", int(res), errMsg));
    }
  }
  throw std::runtime_error(
      std::format("Failed to get image at {}", url_.c_str()));
}

} // namespace video_source