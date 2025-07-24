#pragma once

#include "Gui/Payload.h"

#include <boost/url.hpp>
#include <gsl/gsl>
#include <mongoose.h>
#include <opencv2/core.hpp>

#include <filesystem>
#include <shared_mutex>
#include <string_view>
#include <thread>
#include <vector>

namespace gui {

class WebHandler {
public:
  static void EventHandler(mg_connection *c, int ev, void *ev_data);
  static void
  SetSavedFilesServePath(std::string_view slug,
                         const std::filesystem::path &savedFilesPath);

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

private:
  mg_mgr mgr_;

  boost::url url_;

  struct BroadcastImageData {
    gsl::not_null<mg_mgr *> mgr;
    std::vector<uint8_t> jpgBuf;
    gsl::not_null<std::shared_mutex *> mtx;
    std::array<char, 2> marker{'\0', -1};
  };

  struct FeedImageData {
    cv::Mat imageBgr_;
    cv::Mat modelBgr_;

    std::shared_mutex imageMtx_;
    std::vector<uint8_t> imageJpeg_;
    BroadcastImageData imageBroadcastData_;

    std::shared_mutex modelMtx_;
    std::vector<uint8_t> modelJpeg_;
    BroadcastImageData modelBroadcastData_;

    explicit FeedImageData(mg_mgr *mgr)
        : imageBroadcastData_{.mgr = mgr,
                              .mtx = &modelMtx_,
                              .marker = {'L', -1}},
          modelBroadcastData_{
              .mgr = mgr, .mtx = &imageMtx_, .marker = {'M', -1}} {}
  };

  using BroadcastMap =
      std::unordered_map<std::string_view, std::unique_ptr<FeedImageData>>;
  BroadcastMap feedImageDataMap_;

  static void BroadcastMjpegFrame(gui::WebHandler::BroadcastMap *broadcastData);
  static void BroadcastImage_TimerCallback(void *arg);

  std::jthread listenerThread_;
};

} // namespace gui