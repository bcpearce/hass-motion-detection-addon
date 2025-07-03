#ifndef INCLUDE_HOME_ASSISTANT_ASYNC_REST_HANDLER_H
#define INCLUDE_HOME_ASSISTANT_ASYNC_REST_HANDLER_H

#include "Detector/Detector.h"

#include <memory>
#include <string_view>

#include <boost/url.hpp>

#define JSON_USE_IMPLICIT_CONVERSIONS 0
#include <nlohmann/json.hpp>

namespace home_assistant {

using json = nlohmann::json;

class AsyncHassHandler {

public:
  [[nodiscard]] static std::unique_ptr<AsyncHassHandler>
  Create(boost::url url, std::string token, std::string entityId);

  AsyncHassHandler(boost::url url, std::string token, std::string entityId);
  AsyncHassHandler(const AsyncHassHandler &) = delete;
  AsyncHassHandler(AsyncHassHandler &&) = delete;
  AsyncHassHandler &operator=(const AsyncHassHandler &) = delete;
  AsyncHassHandler &operator=(AsyncHassHandler &&) = delete;

  virtual ~AsyncHassHandler() noexcept = default;

  virtual void
  operator()(std::optional<detector::RegionsOfInterest> rois = {}) = 0;

  std::chrono::duration<double> debounceTime{30.0};
  std::string friendlyName;
  std::string entityId;

protected:
  void UpdateState(std::string_view state, const json &attributes = {});

private:
  boost::url url_;
  std::string token_;

  std::vector<char> buf_;

  json currentState_;
  json nextState_;
};

class AsyncBinarySensorHandler : public AsyncHassHandler {
public:
  using AsyncHassHandler::AsyncHassHandler;
  void
  operator()(std::optional<detector::RegionsOfInterest> rois = {}) override;
};

class AsyncSensorHandler : public AsyncHassHandler {
public:
  using AsyncHassHandler::AsyncHassHandler;
  void
  operator()(std::optional<detector::RegionsOfInterest> rois = {}) override;
};

} // namespace home_assistant

#endif //  INCLUDE_HOMEASSISTANT_HASS_HANDLER_H
