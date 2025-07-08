#include "Logger.h"

#include "HomeAssistant/BaseHassHandler.h"

#include <string>
#include <string_view>

#include "HomeAssistant/Json.h"

using namespace std::string_literals;
using namespace std::string_view_literals;

namespace home_assistant {

BaseHassHandler::BaseHassHandler(const boost::url &url,
                                 const std::string &token,
                                 const std::string &entityId)
    : url_{url}, token_{token}, entityId{entityId} {
  if (std::string_view(GetUrl().host().c_str()) == "supervisor"sv) {
    url_.set_path(std::format("/core/api/states/{}", entityId));
    LOGGER->info("Supervisor detected, setting URL to {}", GetUrl());
  } else {
    url_.set_path(std::format("/api/states/{}", entityId));
    LOGGER->info("Setting URL to {}", GetUrl());
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

void BaseHassHandler::UpdateBinarySensor(
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
  UpdateState(nextState, attributes);
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
    attributes["rois"].push_back(roiJson);
  }
  UpdateState(nextState, attributes);
}

} // namespace home_assistant