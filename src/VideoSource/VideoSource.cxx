#include "VideoSource/VideoSource.h"

#include <ranges>

namespace video_source {

VideoSource::VideoSource() noexcept {
  frame_.timeStamp = std::chrono::steady_clock::now();
}

double VideoSource::GetFramesPerSecond() const {
  // use exponential moving average
  return fps_;
}

void VideoSource::SetFrame(Frame frame) {
  const auto delta = std::chrono::duration_cast<std::chrono::duration<double>>(
      frame.timeStamp - frame_.timeStamp);
  fps_ = 0.1 / delta.count() + fps_ * (1.0 - fpsAlpha);
  frame_ = frame;
  OnEvent(frame_);
}

} // namespace video_source