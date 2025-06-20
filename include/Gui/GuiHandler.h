#ifndef INCLUDE_GUI_HANDLER_H
#define INCLUDE_GUI_HANDLER_H

#include <opencv2/core.hpp>
#include <span>
#include <string>

#include "Detector/Detector.h"
#include "Gui/Payload.h"
#include "VideoSource/VideoSource.h"

namespace gui {

class GuiHandler {

public:
  cv::Mat canvas = cv::Mat::zeros(1, 1, CV_8UC3);
  std::string windowName{"Detail View"};
  bool autoscale{true};

  void operator()(Payload data);

private:
  cv::Mat img;
  cv::Mat model;
  cv::Mat imgPrime;
  cv::Mat modelPrime;
};

} // namespace gui

#endif // !INCLUDE_GUI_HANDLER_H
