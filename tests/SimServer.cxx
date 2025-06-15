#include "SimServer.h"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <filesystem>
#include <format>

namespace {

static std::unique_ptr<SimServer> pServer;
static boost::url url;

static std::jthread listenerThread;

// HTTP server event handler function
void ev_handler(struct mg_connection *c, int ev, void *ev_data) {
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
    }
  }
}

} // namespace

SimServer::SimServer(Token, int port) {
  url.set_scheme("http");
  url.set_host("localhost");
  url.set_port(std::to_string(port));
}

void SimServer::Start(int port) {
  if (!pServer || url.port_number() != port) {
    pServer = std::make_unique<SimServer>(Token(), port);
  }
  listenerThread = std::jthread([](std::stop_token stopToken) {
    struct mg_mgr mgr;
#ifdef _DEBUG
    mg_log_set(MG_LL_DEBUG);
#endif
    mg_mgr_init(&mgr);
    mg_http_listen(&mgr, url.c_str(), ev_handler, nullptr);
    while (!stopToken.stop_requested()) {
      mg_mgr_poll(&mgr, 1000);
    }
    mg_mgr_free(&mgr);
  });
}

void SimServer::Stop() { listenerThread = {}; }

const boost::url &SimServer::GetBaseUrl() { return url; }