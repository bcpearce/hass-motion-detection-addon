#include "WindowsWrapper.h"

#include "Logger.h"

#include "Gui/WebHandler.h"

#include <barrier>
#include <iostream>

#define JSON_USE_IMPLICIT_CONVERSIONS 0
#include <nlohmann/json.hpp>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

using namespace std::string_view_literals;

using json = nlohmann::json;

namespace {

static std::atomic_bool broadcastLogs{false};
static struct mg_mgr *pMgr{nullptr};
static unsigned long parentConnId{0};

static std::shared_mutex feedMappingMtx;
static std::unordered_map<std::string_view, std::filesystem::path>
    savedFilesPath;
static std::unordered_map<std::string_view, char> feedIds;
static std::atomic_char feedMarker{1};

static constexpr const char *mjpegHeaders =
    "HTTP/1.0 200 OK\r\n"
    "Cache-Control: no-cache\r\n"
    "Pragma: no-cache\r\n"
    "Content-Type: multipart/x-mixed-replace; boundary=--boundary\r\n\r\n";

char SafeGetFeedId(mg_str &cap) {
  std::shared_lock lk(feedMappingMtx);
  const auto it = feedIds.find({cap.buf, cap.len});
  if (it != feedIds.end()) {
    return it->second;
  }
  return 0;
}

} // namespace

namespace gui {

void WebHandler::BroadcastMjpegFrame(
    gui::WebHandler::BroadcastMap *broadcastMap) {

  for (const auto &imageData : *broadcastMap | std::views::values) {
    std::array<BroadcastImageData *, 2> ds{&imageData->imageBroadcastData_,
                                           &imageData->modelBroadcastData_};
    for (const auto &broadcastData : ds) {

      mg_mgr *mgr = broadcastData->mgr;
      std::vector<uint8_t> &jpgBuf = broadcastData->jpgBuf;
      const auto marker = broadcastData->marker;
      std::shared_mutex *mtx = broadcastData->mtx;

      for (mg_connection *c = mgr->conns; c != nullptr; c = c->next) {
        if (std::ranges::equal(std::span(c->data, 2), marker) &&
            !jpgBuf.empty()) {
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
  }
}

void WebHandler::BroadcastImage_TimerCallback(void *arg) {
  if (arg) {
    BroadcastMjpegFrame(static_cast<gui::WebHandler::BroadcastMap *>(arg));
  }
}

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
    struct mg_str cap[2] = {mg_str(""), mg_str("")};

    if (mg_match(hm->uri, mg_str("/media/feeds"), nullptr)) {
      json feeds = json::array();
      std::ranges::copy(feedIds | std::views::keys, std::back_inserter(feeds));
      mg_http_reply(c, 200, "Content-Type: application/json\r\n",
                    feeds.dump().c_str());
    } else if (mg_match(hm->uri, mg_str("/media/live/*"), cap)) {
      c->data[0] = 'L';
      c->data[1] = SafeGetFeedId(cap[0]);
      mg_printf(c, "%s", mjpegHeaders);
    } else if (mg_match(hm->uri, mg_str("/media/model/*"), cap)) {
      c->data[0] = 'M';
      c->data[1] = SafeGetFeedId(cap[0]);
      mg_printf(c, "%s", mjpegHeaders);
    } else if (mg_match(hm->uri, mg_str("/websocket"), nullptr)) {
      mg_ws_upgrade(c, hm, nullptr);
      c->data[0] = 'W';
    } else if (mg_match(hm->uri, mg_str("/media/saved/*/*"), cap)) {
      const auto &cSavedFilesPath = savedFilesPath;
      const std::string_view savedFilesSlug(cap[0].buf, cap[0].len);
      if (savedFilesSlug.empty() || !cSavedFilesPath.contains(savedFilesSlug)) {
        mg_http_reply(c, 404, "", "Saved Media Slug Not Found");
        return;
      }
      struct mg_http_serve_opts opts;
      memset(&opts, 0, sizeof(opts));
      thread_local std::string pathStr;
      const std::string_view savedFilesPath(cap[1].buf, cap[1].len);
      if (savedFilesPath.empty() || savedFilesSlug == "null"sv) {
        pathStr = std::format(".,/media/saved/{}/={}", savedFilesSlug,
                              cSavedFilesPath.at(savedFilesSlug).string());
        opts.root_dir = pathStr.c_str();
        mg_http_serve_dir(c, hm, &opts);
      } else {
        pathStr =
            (cSavedFilesPath.at(savedFilesSlug) / savedFilesPath).string();
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
    std::string_view slug, const std::filesystem::path &_savedFilesPath) {
  savedFilesPath[slug] = _savedFilesPath;
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
                 &feedImageDataMap_);
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

        wsMsg["log"] = {};

        wsMsg["log"]["level"] =
            std::string_view(spdlog::level::to_string_view(msg.level));
        wsMsg["log"]["payload"] = std::string_view(msg.payload);
        wsMsg["log"]["timestamp"] = std::format("{}", msg.time);
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

  if (!feedIds.contains(data.feedId) ||
      !feedImageDataMap_.contains(data.feedId)) {
    std::scoped_lock lk(feedMappingMtx);
    if (feedMarker < 0) {
      // maximum feeds of 128, log an error and early exit
      static bool warnOnce{false};
      if (!warnOnce) {
        LOGGER->warn("Maximum feed count reached, cannot add {}", data.feedId);
      }
      return;
    }
    const char thisFeedMarker = feedMarker;

    const auto [feedIdIt, didInsert_feedId] =
        feedIds.insert({data.feedId, thisFeedMarker});
    if (didInsert_feedId) {
      ++feedMarker;
    }
    auto [feedDataIt, didInsert_feedData] = feedImageDataMap_.insert(
        {data.feedId, std::make_unique<FeedImageData>(&mgr_)});
    if (!didInsert_feedData) {
      throw std::runtime_error(
          std::format("Failed to add feed data with ID {}", thisFeedMarker));
    } else {
      feedDataIt->second->imageBroadcastData_.marker[1] = thisFeedMarker;
      feedDataIt->second->modelBroadcastData_.marker[1] = thisFeedMarker;
    }

    LOGGER->info("Feed {} available at Web GUI", data.feedId);
  }

  auto &fi = feedImageDataMap_.at(data.feedId);
  if (!data.frame.img.empty()) {
    switch (data.frame.img.channels()) {
    case 1:
      cv::cvtColor(data.frame.img, fi->imageBgr_, cv::COLOR_GRAY2BGR);
      cv::cvtColor(data.detail, fi->modelBgr_, cv::COLOR_GRAY2BGR);
      break;
    case 3:
      data.frame.img.copyTo(fi->imageBgr_);
      data.detail.copyTo(fi->modelBgr_);
      break;
    case 4:
      cv::cvtColor(data.frame.img, fi->imageBgr_, cv::COLOR_BGRA2BGR);
      cv::cvtColor(data.detail, fi->modelBgr_, cv::COLOR_BGRA2BGR);
      break;
    default:
      throw std::invalid_argument("Frame must be BGR, BGRA, or Monochrome");
    }

    for (const auto &bbox : data.rois) {
      cv::rectangle(fi->imageBgr_, bbox, cv::Scalar(0x00, 0xFF, 0x00), 1);
    }

    thread_local std::string txt;
    txt = std::format(
        "Frame: {} | Objects: {}{}", data.frame.id, data.rois.size(),
        std::isnormal(data.fps) ? std::format(" | FPS: {:.1f}", data.fps) : "");
    cv::Point2i anchor{int(fi->imageBgr_.cols * 0.05),
                       int(fi->imageBgr_.rows * 0.05)};
    cv::putText(fi->modelBgr_, txt, anchor,
                cv::HersheyFonts::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0x00),
                3);
    cv::putText(fi->modelBgr_, txt, anchor,
                cv::HersheyFonts::FONT_HERSHEY_SIMPLEX, 0.5,
                cv::Scalar(0x00, 0xFF, 0xFF), 1);

    if (auto lk = std::unique_lock(fi->imageMtx_)) {
      cv::imencode(".jpg", fi->imageBgr_, fi->imageJpeg_);
      std::swap(fi->imageJpeg_, fi->imageBroadcastData_.jpgBuf);
    }
    if (auto lk = std::unique_lock(fi->modelMtx_)) {
      cv::imencode(".jpg", fi->modelBgr_, fi->modelJpeg_);
      std::swap(fi->modelJpeg_, fi->modelBroadcastData_.jpgBuf);
    }
  }
}

} // namespace gui