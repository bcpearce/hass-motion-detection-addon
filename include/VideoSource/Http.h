#ifndef VIDEO_SOURCE_HTTP_H
#define VIDEO_SOURCE_HTTP_H

#include "WindowsWrapper.h"

#include "Util/CurlWrapper.h"
#include "VideoSource.h"

#include <boost/url.hpp>

#include <array>
#include <chrono>
#include <thread>
#include <vector>

namespace video_source {

class HttpVideoSource : public VideoSource {
public:
  explicit HttpVideoSource(const boost::url &url,
                           const std::string &token = {});
  HttpVideoSource(const boost::url &url, const std::string &username,
                  const std::string &password);
  ~HttpVideoSource() override = default;

  void InitStream() override;
  void StopStream() override;

  Frame GetNextFrame();

  std::chrono::duration<long> timeout{5};

private:
  boost::url url_;
  std::vector<char> buf_;
  util::CurlWrapper wCurl_;
  std::array<char, CURL_ERROR_SIZE> errBuf_;

  std::jthread eventLoopThread_;
};

} // namespace video_source

#endif