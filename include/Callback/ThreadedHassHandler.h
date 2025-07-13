#ifndef INCLUDE_HOME_ASSISTANT_THREADED_HASS_HANDLER_H
#define INCLUDE_HOME_ASSISTANT_THREADED_HASS_HANDLER_H

#include "Detector/Detector.h"

#include "Callback/BaseHassHandler.h"

#include <condition_variable>
#include <memory>
#include <shared_mutex>
#include <string_view>
#include <thread>

#include <boost/url.hpp>

#define JSON_USE_IMPLICIT_CONVERSIONS 0
#include <nlohmann/json.hpp>

namespace callback {

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

  ~ThreadedHassHandler() noexcept override;

protected:
  void UpdateState_Impl(std::string_view state,
                        const json &attributes) override;

private:
  std::vector<char> buf_; // for CURL responses
  std::condition_variable_any cv_;
  std::mutex mtx_;
  std::jthread updaterThread_;
};

} // namespace callback

#endif // INCLUDE_CALLBACK_THREADED_HASS_HANDLER_H
