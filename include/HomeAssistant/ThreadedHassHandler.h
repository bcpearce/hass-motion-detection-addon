#ifndef INCLUDE_HOME_ASSISTANT_THREADED_HASS_HANDLER_H
#define INCLUDE_HOME_ASSISTANT_THREADED_HASS_HANDLER_H

#include "Detector/Detector.h"

#include "HomeAssistant/BaseHassHandler.h"

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

class ThreadedHassHandler : public BaseHassHandler {

public:
  ThreadedHassHandler(const boost::url &url, const std::string &token,
                      const std::string &entityId);
  ThreadedHassHandler(const ThreadedHassHandler &) = delete;
  ThreadedHassHandler(ThreadedHassHandler &&) = delete;
  ThreadedHassHandler &operator=(const ThreadedHassHandler &) = delete;
  ThreadedHassHandler &operator=(ThreadedHassHandler &&) = delete;

  void Start();
  void Stop();

  virtual ~ThreadedHassHandler() noexcept = default;

protected:
  void UpdateState(std::string_view state, const json &attributes) override;

private:
  json currentState_;
  json nextState_;

  std::vector<char> buf_; // for CURL responses
  std::condition_variable_any cv_;
  std::shared_mutex mtx_;
  std::jthread updaterThread_;
};

} // namespace home_assistant

#endif // INCLUDE_HOME_ASSISTANT_THREADED_HASS_HANDLER_H
