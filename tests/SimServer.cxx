#include "SimServer.h"
#include "Logger.h"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#define JSON_USE_IMPLICIT_CONVERSIONS 0
#include <nlohmann/json.hpp>

#include <barrier>
#include <filesystem>
#include <format>
#include <random>

using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;

namespace {

using json = nlohmann::json;

static std::unique_ptr<SimServer> pServer;
static boost::url url;

static std::jthread listenerThread;
static std::atomic_int hassApiCalls_{0};

static std::atomic_int seed{1};

} // namespace

// HTTP server event handler function
void SimServer::ev_handler(struct mg_connection *c, int ev, void *ev_data) {
  if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *hm = (struct mg_http_message *)ev_data;

    if (mg_match(hm->uri, mg_str("/api/getimage"), nullptr)) {
      char argBuf[256];

      mg_str widthStr = mg_http_var(hm->query, mg_str("width"));
      mg_str heightStr = mg_http_var(hm->query, mg_str("height"));
      mg_str shapesStr = mg_http_var(hm->query, mg_str("shapes"));

      const int width = std::stoi(std::string(widthStr.buf, widthStr.len));
      const int height = std::stoi(std::string(heightStr.buf, heightStr.len));
      const int shapes =
          std::string(shapesStr.buf, shapesStr.len).empty()
              ? 90
              : std::stoi(std::string(shapesStr.buf, shapesStr.len));

      thread_local cv::Mat image;
      thread_local std::vector<uint8_t> jpgBuf;

      image = cv::Mat(height, width, CV_8UC3);
      image = cv::Scalar(0, 0, 0);

      // Draw some random shapes
      std::mt19937 rng(std::random_device{}());
      rng.seed(seed++);
      std::uniform_int_distribution<int> distX(0, width);
      std::uniform_int_distribution<int> distY(0, height);
      std::uniform_int_distribution<int> color(0, 255);
      for (int i = 0; i < shapes; ++i) {
        const std::vector<cv::Point> pts = {
            cv::Point(distX(rng), distY(rng)),
            cv::Point(distX(rng), distY(rng)),
            cv::Point(distX(rng), distY(rng)),
        };
        const int diameter = distX(rng);
        switch (i % 3) {
        case 0:
          cv::rectangle(image, pts[0], pts[1],
                        cv::Scalar(color(rng), color(rng), color(rng)), 3);
          break;
        case 1:
          cv::circle(image, pts[0], diameter / 2,
                     cv::Scalar(color(rng), color(rng), color(rng)), 3);
          break;
        case 2: {
          std::vector ptsVec = {pts};
          cv::polylines(image, ptsVec, true,
                        cv::Scalar(color(rng), color(rng), color(rng)), 3);
        } break;
        }
      }

      cv::imencode(".jpg", image, jpgBuf);

      mg_printf(c,
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: image/jpeg\r\n"
                "Content-Length: %ull\r\n\r\n",
                jpgBuf.size());
      mg_send(c, jpgBuf.data(), jpgBuf.size());
      mg_send(c, "\r\n", 2);
    } else if (mg_match(hm->uri, mg_str("/api/hello"), nullptr)) {
      mg_http_reply(c, 200, "", "Hello There");
    } else if (mg_str entity_id[2];
               mg_match(hm->uri, mg_str("/api/states/*"), entity_id)) {

      ++hassApiCalls_; // do before sending
      // handle auth
      std::array<char, 256> user;
      std::array<char, 256> pass;
      user.fill('\0');
      pass.fill('\0');
      mg_http_creds(hm, user.data(), user.size(), pass.data(), pass.size());
      // expect bearer token for hass simulation
      if (std::string_view(user.data()).empty() &&
          std::string_view(pass.data()) ==
              std::string_view(sim_token::bearer)) {
        json responseObj;

        const std::string entityIdStr(entity_id[0].buf, entity_id[0].len);
        if (entityIdStr.ends_with(".missing"sv) &&
            mg_match(hm->message, mg_str("GET"), nullptr)) {
          // special case, respond that the entity is missing, and only for GET
          // requests Post requests should still respond with data
          responseObj["message"] = "Entity not Found"s;
          mg_http_reply(c, 404, "", responseObj.dump().c_str());
        } else if (entityIdStr.starts_with("binary_sensor.")) {
          responseObj["entity_id"] = entityIdStr;
          responseObj["state"] = "on";
          responseObj["attributes"] = {};
          mg_http_reply(c, 200, "", responseObj.dump().c_str());
        } else if (entityIdStr.starts_with("sensor.")) {
          responseObj["entity_id"] = entityIdStr;
          responseObj["state"] = "3";
          responseObj["attributes"] = {};
          mg_http_reply(c, 200, "", responseObj.dump().c_str());
        }
      } else {
        mg_http_reply(c, 401, "", "%s", "401: Unauthorized");
      }
    }
  }
}

SimServer::SimServer(Token, int port) {
  url.set_scheme("http");
  url.set_host("localhost");
  url.set_port(std::to_string(port));
}

static void redirectLog(char c, void *) {
  static std::string buf;
  if (c == '\n') {
    LOGGER->debug("[SIM SERVER] {}", buf);
    buf.clear();
  } else if (c != '\r') {
    buf.push_back(c);
  }
}

void SimServer::Start(int port) noexcept {
  if (!pServer || url.port_number() != port) {
    pServer = std::make_unique<SimServer>(Token(), port);
  }
  std::barrier sync(2);
  listenerThread = std::jthread([&sync](std::stop_token stopToken) {
    struct mg_mgr mgr;
#ifdef _DEBUG
    mg_log_set(MG_LL_DEBUG);
    mg_log_set_fn(redirectLog, nullptr);
#endif
    mg_mgr_init(&mgr);
    mg_connection *c = mg_http_listen(&mgr, url.c_str(), ev_handler, nullptr);
    if (c) {
      LOGGER->info("[SIM SERVER] port set to {}", url.port());
      sync.arrive_and_drop();
      while (!stopToken.stop_requested()) {
        mg_mgr_poll(&mgr, 1000);
      }
    } else {
      sync.arrive_and_drop();
      LOGGER->error("[SIM SERVER] failed to start server on port {}",
                    url.port());
    }
    mg_mgr_free(&mgr);
  });
  sync.arrive_and_wait();
}

void SimServer::Stop() { listenerThread = {}; }

const boost::url &SimServer::GetBaseUrl() { return url; }

int SimServer::GetHassApiCount() { return hassApiCalls_.load(); }

int SimServer::WaitForHassApiCount(int target, std::chrono::seconds timeout) {
  using sc = std::chrono::steady_clock;
  int count = GetHassApiCount();
  for (auto start = sc::now(); sc::now() - start < timeout && count != target;
       count = GetHassApiCount())
    ;
  return count;
}