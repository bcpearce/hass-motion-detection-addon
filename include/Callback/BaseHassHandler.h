#ifndef INCLUDE_HOME_ASSISTANT_BASE_HASS_HANDLER_H
#define INCLUDE_HOME_ASSISTANT_BASE_HASS_HANDLER_H

#include "Detector/Detector.h"
#include "Util/CurlWrapper.h"

#include <span>
#include <string>
#include <vector>

#include <boost/url.hpp>

#define JSON_USE_IMPLICIT_CONVERSIONS 0
#include <nlohmann/json.hpp>

namespace callback {

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

  std::chrono::duration<double> debounceTime{30.0};
  std::string friendlyName;
  std::string entityId;

protected:
  virtual void UpdateState_Impl(std::string_view state,
                                const json &attributes = {}) = 0;

  void UpdateStateInternal(std::string_view state, const json &attributes);
  void UpdateBinarySensor(std::optional<detector::RegionsOfInterest> rois);
  void UpdateSensor(std::optional<detector::RegionsOfInterest> rois);

  [[nodiscard]] bool IsStateChanging() const {
    return nextState_ != currentState_;
  }
  [[nodiscard]] bool IsStateBecomingUnknown() const;

  void PrepareGetRequest(util::CurlWrapper &wCurl, std::vector<char> &buf);
  void HandleGetResponse(util::CurlWrapper &wCurl, std::span<const char> buf);

  void PreparePostRequest(util::CurlWrapper &wCurl, std::vector<char> &readBuf,
                          std::string &payload);
  void HandlePostResponse(util::CurlWrapper &wCurl, std::span<const char> buf);

private:
  boost::url url_;
  std::string token_;

  json currentState_ = {};
  json nextState_ = {};
};

} // namespace callback

#endif //  INCLUDE_HOMEASSISTANT_HASS_HANDLER_H
