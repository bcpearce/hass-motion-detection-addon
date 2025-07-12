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

class AsyncHassHandler : public BaseHassHandler,
                         public std::enable_shared_from_this<AsyncHassHandler> {

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
  void UpdateState_Impl(std::string_view state,
                        const json &attributes) override;

private:
  TaskScheduler *pSched_{nullptr};
  util::CurlMultiWrapper wCurlMulti_;
  TaskToken token_{};

  struct CurlEasyContext {
    util::CurlWrapper wCurl;
    std::vector<char> inBuf;
    std::string outBuf;
    std::string_view outBufVw;
  };

  struct CurlSocketContext : std::enable_shared_from_this<CurlSocketContext> {
    curl_socket_t sockfd{};
    std::weak_ptr<AsyncHassHandler> pHandler;
  };
  std::unordered_map<size_t, std::shared_ptr<CurlEasyContext>> easyCtxs_;
  std::unordered_map<curl_socket_t, std::shared_ptr<CurlSocketContext>>
      socketCtxs_;

  bool allowUpdate_{true};

  void GetInitialState();
  void CheckMultiInfo();

  static int SocketCallback(CURL *easy, curl_socket_t s, int action,
                            AsyncHassHandler *asyncHassHandler,
                            CurlSocketContext *curlSocketContext);
  static int TimeoutCallback(CURLM *multi, int timeoutMs,
                             AsyncHassHandler *asyncHassHandler);

  static void BackgroundHandlerProc(void *curlSocketContext_clientData,
                                    int mask);
  static void TimeoutHandlerProc(void *asyncHassHandler_clientData);
  static void DebounceUpdateProc(void *asyncHassHandler_clientData);
};

} // namespace home_assistant

#endif //  INCLUDE_HOMEASSISTANT_HASS_HANDLER_H
