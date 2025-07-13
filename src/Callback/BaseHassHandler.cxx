#include "Logger.h"

#include "Callback/BaseHassHandler.h"

#include <string>
#include <string_view>

#include "Callback/Json.h"
#include "Util/BufferOperations.h"

using namespace std::string_literals;
using namespace std::string_view_literals;

namespace callback {

BaseHassHandler::BaseHassHandler(const boost::url &url,
                                 const std::string &token,
                                 const std::string &entityId)
    : url_{url}, token_{token}, entityId{entityId} {
  if (std::string_view(url_.host().c_str()) == "supervisor"sv) {
    url_.set_path(std::format("/core/api/states/{}", entityId));
    LOGGER->info("Supervisor detected, setting URL to {}", url_);
  } else {
    url_.set_path(std::format("/api/states/{}", entityId));
    LOGGER->info("Setting URL to {}", url_);
  }
}

void BaseHassHandler::operator()(
    std::optional<detector::RegionsOfInterest> rois) {
  if (entityId.starts_with("binary_sensor."sv)) {
    UpdateBinarySensor(rois);
  } else if (entityId.starts_with("sensor."sv)) {
    UpdateSensor(rois);
  } else {
    throw std::runtime_error(std::format(
        "Unsupported entity type for Home Assistant Handler: {}", entityId));
  }
}

void BaseHassHandler::UpdateStateInternal(std::string_view state,
                                          const json &attributes) {
  nextState_["state"] = state;
  nextState_["attributes"] = attributes;
  if (!friendlyName.empty()) {
    nextState_["attributes"]["friendly_name"] = friendlyName;
  }
}

void BaseHassHandler::UpdateBinarySensor(
    std::optional<detector::RegionsOfInterest> rois) {
  const std::string nextState =
      rois.and_then([](auto r) {
            return r.size() > 0 ? std::optional{"on"s} : std::optional{"off"s};
          })
          .value_or("unknown"s);
  json attributes;
  attributes["device_class"] = "motion";
  for (const auto &roi : rois.value_or(detector::RegionsOfInterest{})) {
    json roiJson;
    to_json(roiJson, roi);
    attributes["rois"].push_back(std::move(roiJson));
  }
  UpdateState_Impl(nextState, attributes);
}

void BaseHassHandler::UpdateSensor(
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
    attributes["rois"].push_back(std::move(roiJson));
  }
  UpdateState_Impl(nextState, attributes);
}

bool BaseHassHandler::IsStateBecomingUnknown() const {
  return nextState_["state"] == "unknown"sv;
}

void BaseHassHandler::PrepareGetRequest(util::CurlWrapper &wCurl,
                                        std::vector<char> &buf) {
  wCurl(curl_easy_setopt, CURLOPT_URL, url_.c_str());
  wCurl(curl_easy_setopt, CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
  wCurl(curl_easy_setopt, CURLOPT_XOAUTH2_BEARER, token_.c_str());
  wCurl(curl_easy_setopt, CURLOPT_HTTPGET, 1L);

  wCurl(curl_easy_setopt, CURLOPT_WRITEFUNCTION, util::FillBufferCallback);
  wCurl(curl_easy_setopt, CURLOPT_WRITEDATA, &buf);
}

void BaseHassHandler::HandleGetResponse(util::CurlWrapper &wCurl,
                                        std::span<const char> buf) {
  int code{0};
  wCurl(curl_easy_getinfo, CURLINFO_RESPONSE_CODE, &code);

  switch (code) {
  case 200:
  case 201: {
    const auto tmpState = json::parse(buf);
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
        contentTypeDesc = std::string_view(buf.data(), buf.size());
    }
    throw std::runtime_error(std::format(
        "Failed get base status of entity {} at {} with code {}: {}",
        url_.c_str(), entityId, code, contentTypeDesc));
  }
  }
}

void BaseHassHandler::PreparePostRequest(util::CurlWrapper &wCurl,
                                         std::vector<char> &buf,
                                         std::string &payload) {
  payload = nextState_.dump();

  wCurl(curl_easy_setopt, CURLOPT_URL, url_.c_str());
  wCurl(curl_easy_setopt, CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
  wCurl(curl_easy_setopt, CURLOPT_XOAUTH2_BEARER, token_.c_str());
  wCurl(curl_easy_setopt, CURLOPT_POST, 1L);

  wCurl(curl_easy_setopt, CURLOPT_WRITEFUNCTION, util::FillBufferCallback);
  wCurl(curl_easy_setopt, CURLOPT_WRITEDATA, &buf);
  wCurl(curl_easy_setopt, CURLOPT_READFUNCTION, util::SendBufferCallback);
  wCurl(curl_easy_setopt, CURLOPT_READDATA, &payload);
}

void BaseHassHandler::HandlePostResponse(util::CurlWrapper &wCurl,
                                         std::span<const char> buf) {
  int code{0};
  wCurl(curl_easy_getinfo, CURLINFO_RESPONSE_CODE, &code);

  switch (code) {
  case 200:
  case 201:
    LOGGER->debug("{}", nextState_.dump(2));
    LOGGER->info("Updated {} with state {}",
                 nextState_["entity_id"].template get<std::string>(),
                 nextState_["state"].template get<std::string>());
    currentState_ = nextState_;
    break;
  default: {
    char *ct{nullptr};
    wCurl(curl_easy_getinfo, CURLINFO_CONTENT_TYPE, &ct);
    std::string_view ctVw{ct};
    std::string_view contentTypeDesc =
        ("application/xml"sv == ctVw || "application/json"sv == ctVw ||
         ctVw.starts_with("text"))
            ? std::string_view(buf.data(), buf.size())
            : ""sv;
    throw std::runtime_error(
        std::format("Failed to update entity {} status with code {}: {}",
                    entityId, code, contentTypeDesc));
  } break;
  }
}

} // namespace callback