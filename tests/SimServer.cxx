#include "SimServer.h"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#define JSON_USE_IMPLICIT_CONVERSIONS 0
#include <nlohmann/json.hpp>

#include <barrier>
#include <filesystem>
#include <format>

using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;

namespace {

using json = nlohmann::json;

static std::unique_ptr<SimServer> pServer;
static boost::url url;

static std::jthread listenerThread;
static std::atomic_int hassApiCalls_{0};

} // namespace

// HTTP server event handler function
void SimServer::ev_handler(struct mg_connection *c, int ev, void *ev_data) {
  if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *hm = (struct mg_http_message *)ev_data;

    if (mg_match(hm->uri, mg_str("/api/getimage"), nullptr)) {
      char argBuf[256];

      struct mg_str widthStr = mg_http_var(hm->query, mg_str("width"));
      struct mg_str heightStr = mg_http_var(hm->query, mg_str("height"));

      int width = std::stoi(std::string(widthStr.buf, widthStr.len));
      int height = std::stoi(std::string(heightStr.buf, heightStr.len));

      thread_local cv::Mat image;
      thread_local std::vector<uint8_t> jpgBuf;

      image = cv::Mat(height, width, CV_8UC3);
      image = cv::Scalar(0xFF, 0xFF, 0xFF);

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

void SimServer::Start(int port) noexcept {
  if (!pServer || url.port_number() != port) {
    pServer = std::make_unique<SimServer>(Token(), port);
  }
  std::barrier sync(2);
  listenerThread = std::jthread([&sync](std::stop_token stopToken) {
    struct mg_mgr mgr;
#ifdef _DEBUG
    mg_log_set(MG_LL_DEBUG);
#endif
    mg_mgr_init(&mgr);
    mg_http_listen(&mgr, url.c_str(), ev_handler, nullptr);
    sync.arrive_and_drop();
    while (!stopToken.stop_requested()) {
      mg_mgr_poll(&mgr, 1000);
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