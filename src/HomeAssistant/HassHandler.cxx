#include "HomeAssistant/HassHandler.h"

#include <chrono>
#include <format>
#include <iostream>

#include "Util/BufferOperations.h"
#include "Util/CurlWrapper.h"

using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;

namespace home_assistant {

std::unique_ptr<HassHandler>
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
    : url_{url}, token_{token} {

  url_.set_path(std::format("/api/states/{}", entityId));

  updaterThread_ = std::jthread([this](std::stop_token stopToken) {
#ifdef _WIN32
    SetThreadDescription(GetCurrentThread(),
                         L"Home Assistant Handler Sensor Update Thread");
#endif

    util::CurlWrapper wCurl;
    wCurl(curl_easy_setopt, CURLOPT_URL, url_.c_str());
    wCurl(curl_easy_setopt, CURLOPT_WRITEFUNCTION, util::FillBufferCallback);
    wCurl(curl_easy_setopt, CURLOPT_WRITEDATA, &buf_);
    wCurl(curl_easy_setopt, CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
    wCurl(curl_easy_setopt, CURLOPT_XOAUTH2_BEARER, token_.c_str());
    wCurl(curl_easy_setopt, CURLOPT_HTTPGET, 1L);
    if (const auto res = wCurl(curl_easy_perform); res == CURLE_OK) {
      const auto tmpState = json::parse(buf_);
      currentState_["state"] = tmpState["state"];
      currentState_["entity_id"] = tmpState["entity_id"];
      currentState_["attributes"] = tmpState["attributes"];
      nextState_ = currentState_;
    }

    std::string payload;
    std::string_view payloadView;

    wCurl(curl_easy_setopt, CURLOPT_POST, 1L);
    wCurl(curl_easy_setopt, CURLOPT_READFUNCTION, util::SendBufferCallback);
    wCurl(curl_easy_setopt, CURLOPT_READDATA, &payloadView);

    using sc = std::chrono::steady_clock;

    // Delay updating the state initially as the feed is still stabilizing
    sc::time_point lastStateUpdate = sc::now() + std::chrono::seconds(30);
    std::unique_lock lk(mtx_);
    do {
      cv_.wait_for(lk, 5s);

      const bool stateChanging = nextState_ != currentState_;
      const bool debounce = (sc::now() - lastStateUpdate > debounceTime);

      if (stateChanging && debounce) {

        buf_.clear();

        payload = nextState_.dump();
        payloadView = payload;

        const auto res = wCurl(curl_easy_perform);
        std::cout << sc::now().time_since_epoch() << " updated hass with state "
                  << nextState_["state"] << "\n";

        currentState_ = nextState_;
        lastStateUpdate = sc::now();
      }
    } while (!stopToken.stop_requested());
  });
}

void HassHandler::UpdateState(std::string_view state, const json &attributes) {
  std::unique_lock lk(mtx_);
  nextState_["state"] = state;
  nextState_["attributes"]["rois"] = attributes;
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