#ifndef INCLUDE_HOME_ASSISTANT_SYNC_HASS_HANDLER_H
#define INCLUDE_HOME_ASSISTANT_SYNC_HASS_HANDLER_H

#include "Detector/Detector.h"

#include "HomeAssistant/BaseHassHandler.h"

#include <string>
#include <string_view>

#include <boost/url.hpp>

namespace home_assistant {

// A very basic handler for Home Assistant State Updates
// This is useful for the final message as there is no requirement to overlap
// the operations with video feed at that point.
class SyncHassHandler : public BaseHassHandler {

public:
  SyncHassHandler(const boost::url &url, const std::string &token,
                  const std::string &entityId);
  SyncHassHandler(const SyncHassHandler &) = delete;
  SyncHassHandler(SyncHassHandler &&) = delete;
  SyncHassHandler &operator=(const SyncHassHandler &) = delete;
  SyncHassHandler &operator=(SyncHassHandler &&) = delete;

  virtual ~SyncHassHandler() noexcept = default;

protected:
  void UpdateState_Impl(std::string_view state,
                        const json &attributes) override;

private:
  std::vector<char> buf_; // for CURL responses
};

} // namespace home_assistant

#endif // INCLUDE_HOME_ASSISTANT_THREADED_HASS_HANDLER_H
