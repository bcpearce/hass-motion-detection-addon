#ifndef INCLUDE_GUI_PAYLOAD_H
#define INCLUDE_GUI_PAYLOAD_H

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
};
} // namespace gui
#endif