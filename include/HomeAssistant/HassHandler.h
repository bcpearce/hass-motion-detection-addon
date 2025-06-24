#ifndef INCLUDE_HOME_ASSISTANT_HASS_HANDLER_H
#define INCLUDE_HOME_ASSISTANT_HASS_HANDLER_H

#include "Detector/Detector.h"

#include <condition_variable>
#include <memory>
#include <shared_mutex>
#include <string_view>
#include <thread>

#include <boost/url.hpp>

#define JSON_USE_IMPLICIT_CONVERSIONS 0
#include <nlohmann/json.hpp>

namespace home_assistant {

using json = nlohmann::json;

class HassHandler {

public:
  [[nodiscard]] static std::unique_ptr<HassHandler>
  Create(const boost::url &url, const std::string &token,
         std::string_view entityId);

  HassHandler(const boost::url &url, const std::string &token,
              std::string_view entityId);
  HassHandler(const HassHandler &) = delete;
  HassHandler(HassHandler &&) = delete;
  HassHandler &operator=(const HassHandler &) = delete;
  HassHandler &operator=(HassHandler &&) = delete;

  virtual ~HassHandler() noexcept = default;

  virtual void
  operator()(std::optional<detector::RegionsOfInterest> rois = {}) = 0;
  std::chrono::duration<double> debounceTime{30.0};

protected:
  void UpdateState(std::string_view state, const json &attributes = {});

private:
  boost::url url_;
  std::string token_;

  std::vector<char> buf_;

  // initialize this true forces initial update
  json currentState_;
  json nextState_;

  std::condition_variable_any cv_;
  std::shared_mutex mtx_;
  std::jthread updaterThread_;
};

class BinarySensorHandler : public HassHandler {
public:
  using HassHandler::HassHandler;
  void
  operator()(std::optional<detector::RegionsOfInterest> rois = {}) override;
};

class SensorHandler : public HassHandler {
public:
  using HassHandler::HassHandler;
  void
  operator()(std::optional<detector::RegionsOfInterest> rois = {}) override;
};

} // namespace home_assistant

#endif //  INCLUDE_HOMEASSISTANT_HASS_HANDLER_H
