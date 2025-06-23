#include "WindowsWrapper.h"

#include "Gui/WebHandler.h"
#include "index.h"

#include <filesystem>
#include <iostream>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace {

static constexpr const char *mjpegHeaders =
    "HTTP/1.0 200 OK\r\n"
    "Cache-Control: no-cache\r\n"
    "Pragma: no-cache\r\n"
    "Content-Type: multipart/x-mixed-replace; boundary=--boundary\r\n\r\n";

void BroadcastMjpegFrame(gui::WebHandler::BroadcastData *broadcastData) {

  mg_mgr *mgr = broadcastData->mgr;
  std::vector<uint8_t> &jpgBuf = broadcastData->jpgBuf;
  const auto marker = broadcastData->marker;
  std::shared_mutex *mtx = broadcastData->mtx;

  for (mg_connection *c = mgr->conns; c != nullptr; c = c->next) {
    if (c->data[0] == marker && !jpgBuf.empty()) {
      std::shared_lock lk(*mtx);
      mg_printf(c,
                "--boundary\r\nContent-Type: image/jpeg\r\n"
                "Content-Length: %lu\r\n\r\n",
                jpgBuf.size());
      mg_send(c, jpgBuf.data(), jpgBuf.size());
      mg_send(c, "\r\n", 2);
    }
  }
}

static void TimerCallback(void *arg) {
  if (arg) {
    BroadcastMjpegFrame(static_cast<gui::WebHandler::BroadcastData *>(arg));
  }
}

// HTTP server event handler function
void EventHandler(mg_connection *c, int ev, void *ev_data) {
  switch (ev) {
  case MG_EV_HTTP_MSG: {
    mg_http_message *hm = (mg_http_message *)ev_data;
    if (mg_match(hm->uri, mg_str("/media/live"), nullptr)) {
      c->data[0] = 'L';
      mg_printf(c, "%s", mjpegHeaders);
    } else if (mg_match(hm->uri, mg_str("/media/model"), nullptr)) {
      c->data[0] = 'M';
      mg_printf(c, "%s", mjpegHeaders);
    } else {
      mg_http_reply(c, 200, "", "%s", web_public::indexHtml);
    }
  } break;
  }
}
} // namespace

namespace gui {

WebHandler::WebHandler(int port, std::string_view host) {
  url_.set_scheme("http");
  url_.set_host(host);
  url_.set_port_number(port);
}

void WebHandler::Start() {
  listenerThread_ = std::jthread([this](std::stop_token stopToken) {
#ifdef _DEBUG
    mg_log_set(MG_LL_DEBUG);
#endif
    mg_mgr_init(&mgr_);
    mg_timer_add(&mgr_, 33, MG_TIMER_REPEAT, TimerCallback,
                 &imageBroadcastData_);
    mg_timer_add(&mgr_, 33, MG_TIMER_REPEAT, TimerCallback,
                 &modelBroadcastData_);
    mg_http_listen(&mgr_, url_.c_str(), EventHandler, nullptr);
    while (!stopToken.stop_requested()) {
      mg_mgr_poll(&mgr_, 100);
    }
    mg_mgr_free(&mgr_);
  });
}

void WebHandler::Stop() { listenerThread_ = {}; }

const boost::url &WebHandler::GetUrl() const noexcept { return url_; }

void WebHandler::operator()(Payload data) {
  switch (data.frame.img.channels()) {
  case 1:
    cv::cvtColor(data.frame.img, imageBgr_, cv::COLOR_GRAY2BGR);
    cv::cvtColor(data.detail, modelBgr_, cv::COLOR_GRAY2BGR);
    break;
  case 3:
    data.frame.img.copyTo(imageBgr_);
    data.detail.copyTo(modelBgr_);
    break;
  case 4:
    cv::cvtColor(data.frame.img, imageBgr_, cv::COLOR_BGRA2BGR);
    cv::cvtColor(data.detail, modelBgr_, cv::COLOR_BGRA2BGR);
    break;
  default:
    throw std::invalid_argument("Frame must be BGR, BGRA, or Monochrome");
  }

  for (const auto &bbox : data.rois) {
    cv::rectangle(imageBgr_, bbox, cv::Scalar(0x00, 0xFF, 0x00), 1);
  }

  thread_local std::string txt;
  txt = std::format(
      "Frame: {} | Objects: {}{}", data.frame.id, data.rois.size(),
      std::isnormal(data.fps) ? std::format(" | FPS: {:.1f}", data.fps) : "");
  cv::Point2i anchor{int(imageBgr_.cols * 0.05), int(imageBgr_.rows * 0.05)};
  cv::putText(modelBgr_, txt, anchor, cv::HersheyFonts::FONT_HERSHEY_SIMPLEX,
              0.5, cv::Scalar(0x00), 3);
  cv::putText(modelBgr_, txt, anchor, cv::HersheyFonts::FONT_HERSHEY_SIMPLEX,
              0.5, cv::Scalar(0x00, 0xFF, 0xFF), 1);

  if (auto lk = std::unique_lock(imageMtx_)) {
    cv::imencode(".jpg", imageBgr_, imageJpeg_);
    std::swap(imageJpeg_, imageBroadcastData_.jpgBuf);
  }
  if (auto lk = std::unique_lock(modelMtx_)) {
    cv::imencode(".jpg", modelBgr_, modelJpeg_);
    std::swap(modelJpeg_, modelBroadcastData_.jpgBuf);
  }
}

} // namespace gui