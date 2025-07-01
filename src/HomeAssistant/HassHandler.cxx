#include "Logger.h"

#include "HomeAssistant/HassHandler.h"

#include <chrono>
#include <format>
#include <iostream>
#include <string_view>

#include "Util/BufferOperations.h"
#include "Util/CurlWrapper.h"

using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;

namespace home_assistant {

[[nodiscard]] std::unique_ptr<HassHandler>
home_assistant::HassHandler::Create(const boost::url &url,
                                    const std::string &token,
                                    std::string_view entityId) {
  if (entityId.starts_with("binary_sensor."sv)) {
    return std::make_unique<BinarySensorHandler>(url, token, entityId);
  } else if (entityId.starts_with("sensor."sv)) {
    return std::make_unique<SensorHandler>(url, token, entityId);
  }
  throw std::runtime_error(std::format(
      "Unsupported entity type for Home Assistant Handler: {}", entityId));
}

HassHandler::HassHandler(const boost::url &url, const std::string &token,
                         std::string_view entityId)
    : url_{url}, token_{token}, entityId{entityId} {}

void HassHandler::Start() {

  if (std::string_view(url_.host().c_str()) == "supervisor"sv) {
    url_.set_path(std::format("/core/api/states/{}", entityId));
    LOGGER->info("Supervisor detected, setting URL to {}", url_);
  } else {
    url_.set_path(std::format("/api/states/{}", entityId));
    LOGGER->info("Setting URL to {}", url_);
  }

  util::CurlWrapper wCurl;
  wCurl(curl_easy_setopt, CURLOPT_URL, url_.c_str());
  wCurl(curl_easy_setopt, CURLOPT_WRITEFUNCTION, util::FillBufferCallback);
  wCurl(curl_easy_setopt, CURLOPT_WRITEDATA, &buf_);
  wCurl(curl_easy_setopt, CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
  wCurl(curl_easy_setopt, CURLOPT_XOAUTH2_BEARER, token_.c_str());
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
          url_.c_str(), entityId, code, contentTypeDesc));
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
void HassHandler::Stop() { updaterThread_ = {}; }

void HassHandler::UpdateState(std::string_view state, const json &attributes) {
  std::unique_lock lk(mtx_);
  nextState_["state"] = state;
  nextState_["attributes"] = attributes;
  if (!friendlyName.empty()) {
    nextState_["attributes"]["friendly_name"] = friendlyName;
  }
  cv_.notify_all();
}

void to_json(json &j, const cv::Rect &rect) {
  j = json{{"x", rect.x},
           {"y", rect.y},
           {"width", rect.width},
           {"height", rect.height}};
}

void from_json(const json &j, cv::Rect &rect) {
  rect.x = j.at("x").get<int>();
  rect.y = j.at("y").get<int>();
  rect.width = j.at("width").get<int>();
  rect.height = j.at("height").get<int>();
}

void BinarySensorHandler::operator()(
    std::optional<detector::RegionsOfInterest> rois) {
  const std::string_view nextState = rois.and_then([](auto r) {
                                           return r.size() > 0
                                                      ? std::optional{"on"sv}
                                                      : std::optional{"off"sv};
                                         })
                                         .value_or("unknown"sv);
  json attributes;
  attributes["device_class"] = "motion";
  for (const auto &roi : rois.value_or(detector::RegionsOfInterest{})) {
    json roiJson;
    to_json(roiJson, roi);
    attributes["rois"].push_back(roiJson);
  }
  HassHandler::UpdateState(nextState, attributes);
}

void SensorHandler::operator()(
    std::optional<detector::RegionsOfInterest> rois) {
  const std::string nextState =
      rois.and_then([](auto r) {
            return std::make_optional(std::to_string(r.size()));
          })
          .value_or("unknown"s);
  json attributes;
  for (const auto &roi : detector::RegionsOfInterest{}) {
    json roiJson;
    to_json(roiJson, roi);
    attributes["rois"].push_back(roiJson);
  }
  HassHandler::UpdateState(nextState, attributes);
}

} // namespace home_assistant