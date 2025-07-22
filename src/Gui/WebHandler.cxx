#include "WindowsWrapper.h"

#include "Logger.h"

#include "Gui/WebHandler.h"

#include <barrier>
#include <iostream>

#define JSON_USE_IMPLICIT_CONVERSIONS 0
#include <nlohmann/json.hpp>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

using json = nlohmann::json;

namespace {

static std::atomic_bool broadcastLogs{false};
static struct mg_mgr *pMgr{nullptr};
static unsigned long parentConnId{0};

static std::unordered_map<int, std::filesystem::path> savedFilesPath;

static constexpr const char *mjpegHeaders =
    "HTTP/1.0 200 OK\r\n"
    "Cache-Control: no-cache\r\n"
    "Pragma: no-cache\r\n"
    "Content-Type: multipart/x-mixed-replace; boundary=--boundary\r\n\r\n";

void BroadcastMjpegFrame(gui::WebHandler::BroadcastImageData *broadcastData) {

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

static void BroadcastImage_TimerCallback(void *arg) {
  if (arg) {
    BroadcastMjpegFrame(
        static_cast<gui::WebHandler::BroadcastImageData *>(arg));
  }
}
} // namespace

namespace gui {

// HTTP server event handler function
void WebHandler::EventHandler(mg_connection *c, int ev, void *ev_data) {
  switch (ev) {
  case MG_EV_OPEN:
    if (c->is_listening) {
      pMgr = c->mgr;
      parentConnId = c->id;
      broadcastLogs = true;
    }
    break;
  case MG_EV_HTTP_MSG: {
    struct mg_http_message *hm = static_cast<mg_http_message *>(ev_data);
    if (mg_match(hm->uri, mg_str("/media/live"), nullptr)) {
      c->data[0] = 'L';
      mg_printf(c, "%s", mjpegHeaders);
    } else if (mg_match(hm->uri, mg_str("/media/model"), nullptr)) {
      c->data[0] = 'M';
      mg_printf(c, "%s", mjpegHeaders);
    } else if (mg_match(hm->uri, mg_str("/websocket"), nullptr)) {
      mg_ws_upgrade(c, hm, nullptr);
      c->data[0] = 'W';
    } else if (struct mg_str cap[2] = {mg_str(""), mg_str("")};
               mg_match(hm->uri, mg_str("/media/saved/*/*"), cap)) {
      const int savedFilesPathIndex =
          std::stoi(std::string(cap[0].buf, cap[0].len));
      struct mg_http_serve_opts opts;
      memset(&opts, 0, sizeof(opts));
      thread_local std::string pathStr;
      if (cap[1].len == 0) {
        pathStr = std::format(".,/media/saved/{}={}", savedFilesPathIndex,
                              savedFilesPath[savedFilesPathIndex].string());
        opts.root_dir = pathStr.c_str();
        mg_http_serve_dir(c, hm, &opts);
      } else {
        pathStr = (savedFilesPath[savedFilesPathIndex] /
                   std::string(cap->buf, cap->len))
                      .string();
        opts.mime_types = "jpg=image/jpg";
        mg_http_serve_file(c, hm, pathStr.c_str(), &opts);
      }
    } else {
      struct mg_http_serve_opts opts;
      memset(&opts, 0, sizeof(opts));
#ifdef SERVE_UNPACKED
      // Use for testing, enables "hot reload" for resources in public folder
      static const auto rootDir =
          std::filesystem::path(__FILE__).parent_path() / "public";
      thread_local std::string pathStr;
      pathStr = rootDir.string();
      opts.root_dir = pathStr.c_str();
#else
      opts.root_dir = "/public";
      opts.fs = &mg_fs_packed;
#endif
      mg_http_serve_dir(c, hm, &opts);
    }
  } break;
  case MG_EV_WAKEUP: {
    const std::string_view msg = *((std::string_view *)ev_data);
    for (mg_connection *wc = c->mgr->conns; wc != nullptr; wc = wc->next) {
      if (wc->data[0] == 'W') {
        mg_ws_send(wc, msg.data(), msg.size(), WEBSOCKET_OP_TEXT);
      }
    }
  } break;
  }
}

void WebHandler::SetSavedFilesServePath(
    int i, const std::filesystem::path &_savedFilesPath) {
  savedFilesPath[i] = _savedFilesPath;
}

WebHandler::WebHandler(int port, std::string_view host) {
  url_.set_scheme("http");
  url_.set_host(host);
  url_.set_port_number(port);
}

void WebHandler::Start() {
  std::barrier sync(2);
  listenerThread_ = std::jthread([this, &sync](std::stop_token stopToken) {
#ifdef _WIN32
    SetThreadDescription(GetCurrentThread(), L"WebHandler Thread");
#endif
#ifdef _DEBUG
    mg_log_set(MG_LL_DEBUG);
#endif
    mg_mgr_init(&mgr_);
    mg_timer_add(&mgr_, 33, MG_TIMER_REPEAT, BroadcastImage_TimerCallback,
                 &imageBroadcastData_);
    mg_timer_add(&mgr_, 33, MG_TIMER_REPEAT, BroadcastImage_TimerCallback,
                 &modelBroadcastData_);
    mg_wakeup_init(&mgr_);
    sync.arrive_and_drop();
    mg_http_listen(&mgr_, url_.c_str(), EventHandler, nullptr);
    while (!stopToken.stop_requested()) {
      mg_mgr_poll(&mgr_, 100);
    }
    mg_mgr_free(&mgr_);
  });

  sync.arrive_and_wait();
  auto pWebsocketSink = std::make_shared<spdlog::sinks::callback_sink_mt>(
      [this](const spdlog::details::log_msg &msg) {
        const auto levelSv =
            std::string_view(spdlog::level::to_string_view(msg.level));
        const auto msgSv = std::string_view(msg.payload);
        thread_local json wsMsg = {};
        thread_local std::string dumped;

        wsMsg["level"] =
            std::string_view(spdlog::level::to_string_view(msg.level));
        wsMsg["payload"] = std::string_view(msg.payload);
        wsMsg["timestamp"] = std::format("{}", msg.time);
        dumped = wsMsg.dump();

        if (broadcastLogs) {
          mg_wakeup(pMgr, parentConnId, dumped.data(), dumped.size());
        }
      });

  LOGGER->debug("Initializing WebSocket sink");
  LOGGER->sinks().push_back(pWebsocketSink);
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