#ifndef VIDEO_SOURCE_LIVE555_H
#define VIDEO_SOURCE_LIVE555_H

#include "WindowsWrapper.h"

#include "VideoSource.h"

#include <thread>

#include <BasicUsageEnvironment.hh>
#include <boost/url.hpp>

namespace video_source {

class FrameRtspClient;

class Live555VideoSource : public VideoSource {

  friend class FrameRtspClient;
  friend class FrameSetterSink;

public:
  Live555VideoSource() noexcept = default;
  explicit Live555VideoSource(const boost::url &url);
  Live555VideoSource(const boost::url &url, std::string_view username,
                     std::string_view password);
  ~Live555VideoSource() override;

  void InitStream() override;
  void StopStream() override;
  [[nodiscard]] bool IsActive() override { return eventLoopWatchVar_ == 0; }

  const boost::url &GetUrl() const { return url_; };

private:
  void SetYUVFrame(uint8_t **pDataYUV, int width, int height, int strideY,
                   int strideUV, int timestamp);

  boost::url url_;
  std::unique_ptr<FrameRtspClient> pRtspClient_;
  TaskScheduler *pScheduler_{nullptr};
  UsageEnvironment *pEnv_{nullptr};
  EventLoopWatchVariable eventLoopWatchVar_{1};

  std::jthread eventLoopThread_;
};

} // namespace video_source
#endif
