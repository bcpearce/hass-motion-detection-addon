#ifndef INCLUDE_GUI_WEBHANDLER_H
#define INCLUDE_GUI_WEBHANDLER_H

#include "Gui/Payload.h"

#include <boost/url.hpp>
#include <gsl/gsl>
#include <mongoose.h>
#include <opencv2/core.hpp>

#include <shared_mutex>
#include <thread>
#include <vector>

namespace gui {

class WebHandler {
public:
  explicit WebHandler(int port, std::string_view host = "0.0.0.0");
  WebHandler(const WebHandler &) = delete;
  WebHandler &operator=(const WebHandler &) = delete;
  WebHandler(WebHandler &&) = delete;
  WebHandler &operator=(WebHandler &&) = delete;

  void Start();
  void Stop();

  const boost::url &GetUrl() const noexcept;

  ~WebHandler() noexcept = default;

  void operator()(Payload data);

  struct BroadcastData {
    gsl::not_null<mg_mgr *> mgr;
    std::vector<uint8_t> jpgBuf;
    gsl::not_null<std::shared_mutex *> mtx;
    char marker{'\0'};
  };

private:
  mg_mgr mgr_;

  boost::url url_;
  std::jthread listenerThread_;

  cv::Mat imageBgr_;
  cv::Mat modelBgr_;

  std::shared_mutex imageMtx_;
  std::vector<uint8_t> imageJpeg_;
  BroadcastData imageBroadcastData_{&mgr_, {}, &imageMtx_, 'L'};

  std::shared_mutex modelMtx_;
  std::vector<uint8_t> modelJpeg_;
  BroadcastData modelBroadcastData_{&mgr_, {}, &modelMtx_, 'M'};
};

} // namespace gui

#endif