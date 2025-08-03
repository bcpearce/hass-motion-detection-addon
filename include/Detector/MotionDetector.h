#pragma once

#include "Detector/Detector.h"

#include <variant>

#include <opencv2/bgsegm.hpp>
#include <opencv2/core.hpp>

namespace detector {

class BasicMotionDetector : public Detector {
public:
  struct Options {
    double detectionLimit{50};
    double alpha{0.05};
    std::variant<int, double> detectionSize{500};
  };

  explicit BasicMotionDetector(Options options);
  virtual ~BasicMotionDetector() noexcept = default;

  std::variant<int, double> GetDetectionSize() override {
    return options.detectionSize;
  }

  void ResetModel() override;
  cv::Mat GetModel() override;

  Options options;

private:
  std::span<const cv::Rect> FeedFrame_Impl(cv::Mat frame) override;

  cv::Mat monoFrame_;
  cv::Mat bgModel_;
  cv::Mat absDiff_;
  cv::Mat thresh_;
  std::vector<cv::Rect> contourBounds_;
  size_t frameCount_{0};
};

class MOGMotionDetector : public Detector {
public:
  struct Options {
    int history{200};
    int nmixtures{5};
    double backgroundRatio{0.7};
    double noiseSigma{0.0};
    double learningRate{-1.0};
    std::variant<int, double> detectionSize{500};
  };

  explicit MOGMotionDetector(Options options);
  virtual ~MOGMotionDetector() noexcept = default;

  std::variant<int, double> GetDetectionSize() override {
    return options.detectionSize;
  }

  void ResetModel() override;
  cv::Mat GetModel() override;
  const cv::Mat &GetMonoFrame() const { return monoFrame_; }

  Options options;

private:
  std::span<const cv::Rect> FeedFrame_Impl(cv::Mat frame) override;
  void ResetModel_Impl();

  cv::Ptr<cv::BackgroundSubtractor> pBgsegm_{nullptr};
  cv::Mat monoFrame_;
  cv::Mat fgMask_;
  std::vector<cv::Rect> contourBounds_;
  size_t frameCount_{0};
};

} // namespace detector
