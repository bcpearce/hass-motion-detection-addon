#include "Logger.h"

#include "HomeAssistant/ThreadedHassHandler.h"

#include <chrono>
#include <format>
#include <iostream>
#include <string_view>

#include "HomeAssistant/Json.h"
#include "Util/BufferOperations.h"
#include "Util/CurlWrapper.h"

using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;

namespace home_assistant {

ThreadedHassHandler::ThreadedHassHandler(const boost::url &url,
                                         const std::string &token,
                                         const std::string &entityId)
    : BaseHassHandler(url, token, entityId) {}

void ThreadedHassHandler::Start() {
  util::CurlWrapper wCurl;
  wCurl(curl_easy_setopt, CURLOPT_URL, GetUrl().c_str());
  wCurl(curl_easy_setopt, CURLOPT_WRITEFUNCTION, util::FillBufferCallback);
  wCurl(curl_easy_setopt, CURLOPT_WRITEDATA, &buf_);
  wCurl(curl_easy_setopt, CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
  wCurl(curl_easy_setopt, CURLOPT_XOAUTH2_BEARER, GetToken().c_str());
  wCurl(curl_easy_setopt, CURLOPT_HTTPGET, 1L);
  if (const auto res = wCurl(curl_easy_perform); res == CURLE_OK) {
    int code{0};
    wCurl(curl_easy_getinfo, CURLINFO_RESPONSE_CODE, &code);

    switch (code) {
    case 200:
    case 201: {
      const auto tmpState = json::parse(buf_);
      currentState_["state"] = tmpState["state"];
      currentState_["entity_id"] = tmpState["entity_id"];
      currentState_["attributes"] = tmpState["attributes"];
      nextState_ = currentState_;
    } break;
    case 404:
      // Not found, entity will be created upon post
      LOGGER->warn(
          "Entity with ID {} was not found, will be created upon first POST",
          entityId);
      currentState_["state"] = "unknown";
      currentState_["entity_id"] = entityId;
      currentState_["attributes"] = {};
      nextState_ = currentState_;
      break;
    default: {
      char *ct{nullptr};
      wCurl(curl_easy_getinfo, CURLINFO_CONTENT_TYPE, &ct);
      std::string_view contentTypeDesc = ""sv;
      if (ct) {
        std::string_view ctVw{ct};
        if ("application/xml"sv == ctVw || "application/json"sv == ctVw ||
            ctVw.starts_with("text"))
          contentTypeDesc = std::string_view(buf_.data(), buf_.size());
      }
      throw std::runtime_error(std::format(
          "Failed get base status of entity {} at {} with code {}: {}",
          GetUrl().c_str(), entityId, code, contentTypeDesc));
    }
    }
  }

  updaterThread_ = std::jthread([this, wCurl = std::move(wCurl)](
                                    std::stop_token stopToken) mutable {
#ifdef _WIN32
    SetThreadDescription(GetCurrentThread(),
                         L"Home Assistant Handler Sensor Update Thread");
#endif

    std::string payload;
    std::string_view payloadView;

    wCurl(curl_easy_setopt, CURLOPT_POST, 1L);
    wCurl(curl_easy_setopt, CURLOPT_READFUNCTION, util::SendBufferCallback);
    wCurl(curl_easy_setopt, CURLOPT_READDATA, &payloadView);

    using sc = std::chrono::steady_clock;

    // Delay updating the state initially as the feed is still stabilizing
    sc::time_point lastStateUpdate =
        sc::now() +
        std::chrono::duration_cast<std::chrono::seconds>(debounceTime);
    std::unique_lock lk(mtx_);
    do {
      cv_.wait_for(lk, 5s);

      const bool stateChanging = nextState_ != currentState_;
      const bool debounce = (sc::now() - lastStateUpdate > debounceTime);

      if ((stateChanging && debounce) || nextState_["state"] == "unknown"sv) {

        buf_.clear();

        payload = nextState_.dump();
        payloadView = payload;
        try {
          wCurl(curl_easy_perform);
          int code{0};
          wCurl(curl_easy_getinfo, CURLINFO_RESPONSE_CODE, &code);

          switch (code) {
          case 200:
          case 201:
            LOGGER->info("Updated {} with state {}",
                         nextState_["entity_id"].template get<std::string>(),
                         nextState_["state"].template get<std::string>());
            LOGGER->debug("{}", nextState_.dump(2));
            currentState_ = nextState_;
            lastStateUpdate = sc::now();
            break;
          default: {
            char *ct{nullptr};
            wCurl(curl_easy_getinfo, CURLINFO_CONTENT_TYPE, &ct);
            std::string_view ctVw{ct};
            std::string_view contentTypeDesc =
                ("application/xml"sv == ctVw || "application/json"sv == ctVw ||
                 ctVw.starts_with("text"))
                    ? std::string_view(buf_.data(), buf_.size())
                    : ""sv;
            LOGGER->error("Failed to update entity {} status with code {}: {}",
                          entityId, code, contentTypeDesc);
          } break;
          }
        } catch (const std::exception &e) {
          LOGGER->error(e.what());
        }
      }
    } while (!stopToken.stop_requested());
  });
}

void ThreadedHassHandler::Stop() { updaterThread_ = {}; }

void ThreadedHassHandler::UpdateState(std::string_view state,
                                      const json &attributes) {
  std::unique_lock lk(mtx_);
  nextState_["state"] = state;
  nextState_["attributes"] = attributes;
  if (!friendlyName.empty()) {
    nextState_["attributes"]["friendly_name"] = friendlyName;
  }
  cv_.notify_all();
}

} // namespace home_assistant