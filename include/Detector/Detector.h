#pragma once

#include <span>
#include <variant>
#include <vector>

#include <opencv2/core.hpp>

#include "Util/EventHandler.h"
#include "VideoSource/VideoSource.h"

namespace detector {

using RegionsOfInterest = std::span<const cv::Rect>;

struct Payload {
  video_source::Frame frame;
  cv::Mat mask;
  RegionsOfInterest rois;
};

class Detector : public util::EventHandler<Payload> {

public:
  Detector() noexcept = default;
  virtual ~Detector() noexcept = default;

  virtual std::variant<int, double> GetDetectionSize() = 0;

  RegionsOfInterest FeedFrame(video_source::Frame frame);
  [[nodiscard]] RegionsOfInterest GetRois() const { return rois_; };

  virtual void ResetModel() = 0;
  [[nodiscard]] virtual cv::Mat GetModel() = 0;

  cv::Mat mask;

protected:
  void SetRois(RegionsOfInterest rois);

private:
  virtual RegionsOfInterest FeedFrame_Impl(cv::Mat frame) = 0;
  RegionsOfInterest rois_;
  video_source::Frame frame_;
  cv::Mat maskedFrame_;
};

} // namespace detector
