#include "Detector/Detector.h"

namespace detector {

RegionsOfInterest Detector::FeedFrame(video_source::Frame frame) {
  frame_ = frame;
  if (!mask.empty()) {
    cv::bitwise_and(frame.img, mask, maskedFrame_);
    return FeedFrame_Impl(maskedFrame_);
  }
  rois_ = FeedFrame_Impl(frame.img);
  return rois_;
}

void Detector::SetRois(RegionsOfInterest rois) {
  rois_ = rois;
  OnEvent({.frame = frame_, .mask = mask, .rois = rois});
}

} // namespace detector