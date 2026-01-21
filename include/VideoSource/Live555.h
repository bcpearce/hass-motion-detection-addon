#pragma once

#include "WindowsWrapper.h"

#include "VideoSource.h"

#include <thread>

#include <UsageEnvironment.hh>
#include <boost/url.hpp>
#include <gsl/gsl>

namespace video_source {

class FrameRtspClient;

class Live555VideoSource : public VideoSource {

  friend class FrameRtspClient;
  friend class FrameSetterSink;

public:
  Live555VideoSource() noexcept = delete;
  Live555VideoSource(std::shared_ptr<TaskScheduler> pSched,
                     const boost::url &url, std::string_view username = {},
                     std::string_view password = {});
  ~Live555VideoSource() override;

  void StartStream(unsigned long long maxFrames =
                       std::numeric_limits<unsigned long long>::max()) override;
  void StopStream() override;
  [[nodiscard]] bool IsActive() override { return bool(pRtspClient_); }

  const boost::url &GetUrl() const { return url_; };

private:
  void SetYUVFrame(uint8_t **pDataYUV, int width, int height, int strideY,
                   int strideUV, int timestamp);
  void StopStream_Impl();

  unsigned long long maxFrames_{std::numeric_limits<unsigned long long>::max()};

  boost::url url_;
  std::unique_ptr<FrameRtspClient> pRtspClient_;
  gsl::not_null<std::shared_ptr<TaskScheduler>> pSched_;
  UsageEnvironment *pEnv_{nullptr};
};

} // namespace video_source
