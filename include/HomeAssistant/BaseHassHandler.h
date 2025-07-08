#ifndef INCLUDE_HOME_ASSISTANT_BASE_HASS_HANDLER_H
#define INCLUDE_HOME_ASSISTANT_BASE_HASS_HANDLER_H

#include "Detector/Detector.h"

#include <string>

#include <boost/url.hpp>

#define JSON_USE_IMPLICIT_CONVERSIONS 0
#include <nlohmann/json.hpp>

namespace home_assistant {

using json = nlohmann::json;

class BaseHassHandler {

public:
  BaseHassHandler(const boost::url &url, const std::string &token,
                  const std::string &entityId);
  BaseHassHandler(const BaseHassHandler &) = delete;
  BaseHassHandler(BaseHassHandler &&) = delete;
  BaseHassHandler &operator=(const BaseHassHandler &) = delete;
  BaseHassHandler &operator=(BaseHassHandler &&) = delete;

  virtual ~BaseHassHandler() noexcept = default;

  void operator()(std::optional<detector::RegionsOfInterest> rois = {});

  const boost::url &GetUrl() const { return url_; }
  const std::string &GetToken() const { return token_; }

  std::chrono::duration<double> debounceTime{30.0};
  std::string friendlyName;
  std::string entityId;

protected:
  virtual void UpdateState(std::string_view state,
                           const json &attributes = {}) = 0;
  void UpdateBinarySensor(std::optional<detector::RegionsOfInterest> rois);
  void UpdateSensor(std::optional<detector::RegionsOfInterest> rois);

private:
  boost::url url_;
  std::string token_;
};

} // namespace home_assistant

#endif //  INCLUDE_HOMEASSISTANT_HASS_HANDLER_H
