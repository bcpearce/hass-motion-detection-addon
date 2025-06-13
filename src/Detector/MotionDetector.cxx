#include "Detector/MotionDetector.h"

#include <ranges>

#include <opencv2/imgproc.hpp>

namespace {

static void fillOrSwapMonochrome(cv::Mat &frame, cv::Mat &monoFrame) {
  switch (frame.channels()) {
  case 1:
    if (monoFrame.empty()) {
      frame.copyTo(monoFrame);
    } else {
      std::swap(frame, monoFrame);
    }
    break;
  case 3:
    cv::cvtColor(frame, monoFrame, cv::COLOR_BGR2GRAY);
    break;
  case 4:
    cv::cvtColor(frame, monoFrame, cv::COLOR_BGRA2GRAY);
    break;
  default:
    throw std::invalid_argument("Frame must be BGR, BGRA, or Monochrome");
  }
}

class DetectionSizeVisitor {

public:
  DetectionSizeVisitor(detector::Detector &detector) : rDetector_{detector} {}

  int operator()(int pixels) { return pixels; }
  int operator()(double fractionOfTotalPixels) {
    const int totalPixels =
        rDetector_.GetModel().rows * rDetector_.GetModel().cols;
    return std::clamp(
        static_cast<int>(std::lround(totalPixels * fractionOfTotalPixels)), 0,
        totalPixels);
  }

private:
  detector::Detector &rDetector_;
};

static void RoisFromModel(detector::Detector &detector,
                          std::vector<cv::Rect> &rois) {

  thread_local cv::Mat morph;
  thread_local std::vector<std::vector<cv::Point2i>> contours;
  thread_local std::vector<cv::Rect> contourBounds;

  cv::morphologyEx(detector.GetModel(), morph, cv::MORPH_DILATE,
                   cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5)));

  cv::findContours(morph, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

  contourBounds.clear();

  auto contourFilter =
      [minContourArea = std::visit(DetectionSizeVisitor(detector),
                                   detector.GetDetectionSize())](
          const auto &c) { return cv::contourArea(c) > minContourArea; };

  std::ranges::transform(contours | std::views::filter(contourFilter),
                         std::back_inserter(contourBounds),
                         [](const auto &c) { return cv::boundingRect(c); });
  std::swap(rois, contourBounds);
}

} // namespace

namespace detector {

BasicMotionDetector::BasicMotionDetector(Options options) : options{options} {}

std::span<const cv::Rect> BasicMotionDetector::FeedFrame_Impl(cv::Mat frame) {
  // convert to gray
  fillOrSwapMonochrome(frame, monoFrame_);

  // initialize the  model if necessary
  if (bgModel_.empty()) {
    bgModel_ = cv::Mat::zeros(monoFrame_.size(), monoFrame_.type());
  }

  // determine how much of the new frame to mix in
  const double alphaPrime =
      frameCount_ < (1.0 / options.alpha) ? 1.0 / frameCount_ : options.alpha;
  cv::addWeighted(monoFrame_, alphaPrime, bgModel_, 1.0 - alphaPrime, 0,
                  bgModel_);

  // find the changes
  cv::absdiff(monoFrame_, bgModel_, absDiff_);
  cv::threshold(absDiff_, thresh_, options.detectionLimit, 255,
                cv::THRESH_BINARY);

  RoisFromModel(*this, contourBounds_);
  SetRois(contourBounds_);
  ++frameCount_;
  return GetRois();
}

void BasicMotionDetector::ResetModel() {
  bgModel_ = cv::Mat();
  frameCount_ = 0;
  SetRois({});
}

cv::Mat BasicMotionDetector::GetModel() { return bgModel_; }

MOGMotionDetector::MOGMotionDetector(Options options) : options{options} {
  ResetModel();
}

std::span<const cv::Rect> MOGMotionDetector::FeedFrame_Impl(cv::Mat frame) {
  if (!pBgsegm_) {
    ResetModel();
  }

  fillOrSwapMonochrome(frame, monoFrame_);

  pBgsegm_->apply(monoFrame_, fgMask_, options.learningRate);

  // find the changes
  RoisFromModel(*this, contourBounds_);
  SetRois(contourBounds_);
  ++frameCount_;
  return GetRois();
}

void MOGMotionDetector::ResetModel() {
  pBgsegm_ = cv::bgsegm::createBackgroundSubtractorMOG(
      options.history, options.nmixtures, options.backgroundRatio,
      options.noiseSigma);
  frameCount_ = 0;
  SetRois({});
}

cv::Mat MOGMotionDetector::GetModel() { return fgMask_; }

} // namespace detector