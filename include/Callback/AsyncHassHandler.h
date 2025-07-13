#ifndef INCLUDE_CALLBACK_ASYNC_HASS_HANDLER_H
#define INCLUDE_CALLBACK_ASYNC_HASS_HANDLER_H

#include "Callback/BaseHassHandler.h"
#include "Callback/Context.h"

#include <memory>
#include <string_view>
#include <unordered_map>

#include <UsageEnvironment.hh>
#include <boost/url.hpp>

#include "Util/CurlMultiWrapper.h"
#include "Util/CurlWrapper.h"

namespace callback {

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

  using _CurlEasyContext = CurlEasyContext<std::vector<char>, std::string>;
  using _CurlSocketContext = CurlSocketContext<AsyncHassHandler>;
  std::unordered_map<size_t, std::shared_ptr<_CurlEasyContext>> easyCtxs_;
  std::unordered_map<curl_socket_t, std::shared_ptr<_CurlSocketContext>>
      socketCtxs_;

  bool allowUpdate_{true};

  void GetInitialState();
  void CheckMultiInfo();

  static int SocketCallback(CURL *easy, curl_socket_t s, int action,
                            AsyncHassHandler *asyncHassHandler,
                            _CurlSocketContext *curlSocketContext);
  static int TimeoutCallback(CURLM *multi, int timeoutMs,
                             AsyncHassHandler *asyncHassHandler);

  static void BackgroundHandlerProc(void *curlSocketContext_clientData,
                                    int mask);
  static void TimeoutHandlerProc(void *asyncHassHandler_clientData);
  static void DebounceUpdateProc(void *asyncHassHandler_clientData);
};

} // namespace callback

#endif //  INCLUDE_HOMEASSISTANT_HASS_HANDLER_H
