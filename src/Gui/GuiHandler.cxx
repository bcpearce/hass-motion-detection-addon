#include "WindowsWrapper.h"

#include "Gui/GuiHandler.h"

#include <format>
#include <mutex>

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

namespace {

static int gWidth{0};
static int gHeight{0};
std::once_flag screenSizeFlag;

} // namespace

namespace gui {

void GuiHandler::operator()(Payload data) {

  std::call_once(screenSizeFlag, [] {
#ifdef __linux__
    Display *d = XOpenDisplay(NULL);
    Screen *s = DefaultScreenOfDisplay(d);
    gHeight = s->height;
    gWidth = s->width;
#elif _WIN32
  gHeight = GetSystemMetrics(SM_CYSCREEN);
  gWidth = GetSystemMetrics(SM_CXSCREEN);
#endif
  });

  if (data.frame.img.empty()) {
    return;
  }

  cv::cvtColor(data.frame.img, img, cv::COLOR_GRAY2BGR);
  cv::cvtColor(data.detail, model, cv::COLOR_GRAY2BGR);

  for (const auto &bbox : data.rois) {
    cv::rectangle(img, bbox, cv::Scalar(0x00, 0xFF, 0x00), 1);
  }

  thread_local std::string txt;
  txt = std::format(
      "Frame: {} | Objects: {}{}", data.frame.id, data.rois.size(),
      std::isnormal(data.fps) ? std::format(" | FPS: {:.1f}", data.fps) : "");
  cv::Point2i anchor{int(img.cols * 0.05), int(img.rows * 0.05)};
  cv::putText(model, txt, anchor, cv::HersheyFonts::FONT_HERSHEY_SIMPLEX, 0.5,
              cv::Scalar(0x00), 3);
  cv::putText(model, txt, anchor, cv::HersheyFonts::FONT_HERSHEY_SIMPLEX, 0.5,
              cv::Scalar(0x00, 0xFF, 0xFF), 1);

  if (autoscale && data.frame.img.rows > gHeight / 2) {
    const int height = gHeight / 2;
    const int width = data.frame.img.cols * height / data.frame.img.rows;
    cv::resize(img, imgPrime, cv::Size(width, height));
    cv::resize(model, modelPrime, cv::Size(width, height));
  } else {
    imgPrime = img;
    modelPrime = model;
  }

  std::array ia{imgPrime, modelPrime};
  cv::hconcat(ia, canvas);
}

} // namespace gui