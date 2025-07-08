#ifndef INCLUDE_HOME_ASSISTANT_ASYNC_HASS_HANDLER_H
#define INCLUDE_HOME_ASSISTANT_ASYNC_HASS_HANDLER_H

#include "Detector/Detector.h"

#include "HomeAssistant/BaseHassHandler.h"

#include <memory>
#include <string_view>
#include <unordered_map>

#include <UsageEnvironment.hh>
#include <boost/url.hpp>

#define JSON_USE_IMPLICIT_CONVERSIONS 0
#include <nlohmann/json.hpp>

#include "Util/CurlMultiWrapper.h"
#include "Util/CurlWrapper.h"

namespace home_assistant {

using json = nlohmann::json;

class AsyncHassHandler : public BaseHassHandler {

public:
  AsyncHassHandler(const boost::url &url, const std::string &token,
                   const std::string &entityId);
  AsyncHassHandler(const AsyncHassHandler &) = delete;
  AsyncHassHandler(AsyncHassHandler &&) = delete;
  AsyncHassHandler &operator=(const AsyncHassHandler &) = delete;
  AsyncHassHandler &operator=(AsyncHassHandler &&) = delete;

  virtual ~AsyncHassHandler() noexcept = default;

  void Register(TaskScheduler *pSched);

protected:
  void UpdateState(std::string_view state, const json &attributes) override;

private:
  struct CurlMultiContext {
    TaskScheduler *pSched_{nullptr};
    AsyncHassHandler *pHandler{nullptr};
    util::CurlMultiWrapper wCurlMulti_;
    TaskToken token_{};
  };
  CurlMultiContext curlMultiCtx_;

  struct CurlEasyContext {
    TaskScheduler *pSched{nullptr};
    util::CurlWrapper wCurl;
    std::vector<char> buf;
  };

  struct CurlSocketContext {
    curl_socket_t sockfd{};
    CurlMultiContext *pMultiContext{nullptr};
  };
  std::unordered_map<size_t, std::shared_ptr<CurlEasyContext>> ctxs_;

  json currentState_;
  json nextState_;
  bool allowUpdate_{true};

  static int SocketCallback(CURL *easy, curl_socket_t s, int action,
                            CurlMultiContext *mc, void *socketp);
  static int TimeoutCallback(CURLM *multi, int timeoutMs, CurlMultiContext *mc);

  static void CheckMultiInfo(CurlSocketContext &ctx);

  static void BackgroundHandlerProc(void *clientData, int mask);
  static void TimeoutHandlerProc(void *clientData);
  static void DebounceUpdateProc(void *clientData);
};

} // namespace home_assistant

#endif //  INCLUDE_HOMEASSISTANT_HASS_HANDLER_H
