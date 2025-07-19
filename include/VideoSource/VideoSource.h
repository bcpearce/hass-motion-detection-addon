#pragma once

#include <chrono>
#include <string>

#include <opencv2/core.hpp>

#include <Util/EventHandler.h>

namespace video_source {

struct Frame {
  size_t id{0};
  cv::Mat img;
  std::chrono::steady_clock::time_point timeStamp;
};

class VideoSource : public util::EventHandler<Frame> {
public:
  VideoSource() noexcept;
  virtual ~VideoSource() noexcept = default;

  // The stream runs in the thread that starts it.
  // Another thread must call StopStream
  virtual void
  StartStream(unsigned long long maxFrames =
                  std::numeric_limits<unsigned long long>::max()) = 0;
  virtual void StopStream() = 0;
  [[nodiscard]] virtual bool IsActive() = 0;
  [[nodiscard]] Frame GetCurrentFrame() { return frame_; };
  [[nodiscard]] double GetFramesPerSecond() const;
  [[nodiscard]] unsigned long long GetFrameCount() const { return frameCount_; }

  double fpsAlpha{0.1};
  bool fullColor{false};

protected:
  void SetFrame(Frame frame);

private:
  unsigned long long frameCount_{0};
  Frame frame_;
  double fps_{0.0};
};

} // namespace video_source
