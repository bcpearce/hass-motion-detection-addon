#ifndef INCLUDE_VIDEO_SOURCE_H
#define INCLUDE_VIDEO_SOURCE_H

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

  virtual void InitStream() = 0;
  virtual void StopStream() = 0;
  [[nodiscard]] virtual bool IsActive() = 0;
  [[nodiscard]] Frame GetCurrentFrame() { return frame_; };
  [[nodiscard]] double GetFramesPerSecond() const;

  double fpsAlpha{0.1};

protected:
  void SetFrame(Frame frame);

private:
  Frame frame_;
  double fps_{0.0};
};

} // namespace video_source
#endif
