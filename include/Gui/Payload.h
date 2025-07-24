#pragma once

#include <limits>

#include <opencv2/core.hpp>

#include "Detector/Detector.h"
#include "VideoSource/VideoSource.h"

namespace gui {
struct Payload {
  detector::RegionsOfInterest rois;
  video_source::Frame frame;
  cv::Mat detail;
  double fps{std::numeric_limits<double>::quiet_NaN()};
  std::string_view feedId;
};
} // namespace gui