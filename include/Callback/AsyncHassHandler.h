#pragma once

#include "Callback/AsyncDebouncer.h"
#include "Callback/BaseHassHandler.h"
#include "Callback/Context.h"

#include <gsl/gsl>
#include <memory>
#include <string_view>
#include <unordered_map>

#include <UsageEnvironment.hh>
#include <boost/url.hpp>

#include "Util/CurlMultiWrapper.h"
#include "Util/CurlWrapper.h"

namespace callback {

class AsyncHassHandler : public BaseHassHandler,
                         protected AsyncDebouncer,
                         public std::enable_shared_from_this<AsyncHassHandler> {

public:
  AsyncHassHandler(std::shared_ptr<TaskScheduler> pSched, const boost::url &url,
                   const std::string &token, const std::string &entityId);
  AsyncHassHandler(const AsyncHassHandler &) = delete;
  AsyncHassHandler(AsyncHassHandler &&) = delete;
  AsyncHassHandler &operator=(const AsyncHassHandler &) = delete;
  AsyncHassHandler &operator=(AsyncHassHandler &&) = delete;

  virtual ~AsyncHassHandler() noexcept;

  void Register();

protected:
  void UpdateState_Impl(std::string_view state,
                        const json &attributes) override;

private:
  gsl::not_null<std::shared_ptr<TaskScheduler>> pSched_;
  util::CurlMultiWrapper wCurlMulti_;
  TaskToken timeoutTaskToken_{};

  using _CurlEasyContext = CurlEasyContext<std::vector<char>, std::string>;
  using _CurlSocketContext = CurlSocketContext<AsyncHassHandler>;
  std::unordered_map<size_t, std::shared_ptr<_CurlEasyContext>> easyCtxs_;
  std::unordered_map<curl_socket_t, std::shared_ptr<_CurlSocketContext>>
      socketCtxs_;

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
};

} // namespace callback
