#include "Logger.h"
#include "WindowsWrapper.h"

#include "HomeAssistant/AsyncHassHandler.h"
#include <UsageEnvironment.hh>

#include "HomeAssistant/Json.h"
#include "Util/BufferOperations.h"
#include "Util/CurlWrapper.h"

#include <iostream>

#include <string_view>

using namespace std::string_literals;
using namespace std::string_view_literals;

namespace home_assistant {

int AsyncHassHandler::SocketCallback(CURL *easy, curl_socket_t s, int action,
                                     CurlMultiContext *mc, void *socketp) {
  switch (action) {
  case CURL_POLL_IN:
  case CURL_POLL_OUT:
  case CURL_POLL_INOUT: {
    CurlSocketContext *pCtx{nullptr};
    if (pCtx) {
      pCtx = static_cast<CurlSocketContext *>(socketp);
    } else {
      auto pNewSocketCtx = std::make_unique<CurlSocketContext>(s, mc);
      pCtx = pNewSocketCtx.release();
    }

    mc->wCurlMulti_(curl_multi_assign, s, static_cast<void *>(pCtx));

    int flags{0};
    flags |= (action != CURL_POLL_IN) ? SOCKET_WRITABLE : 0;
    flags |= (action != CURL_POLL_OUT) ? SOCKET_READABLE : 0;

    mc->pSched_->setBackgroundHandling(
        s, flags, AsyncHassHandler::BackgroundHandlerProc, pCtx);
  } break;
  case CURL_POLL_REMOVE:
    if (socketp) {
      mc->pSched_->disableBackgroundHandling(s);
      CurlSocketContext *pCtx = static_cast<CurlSocketContext *>(socketp);
      auto upCtx = std::unique_ptr<CurlSocketContext>(
          pCtx); // take ownership to destroy on scop exit
      upCtx->pMultiContext->wCurlMulti_(curl_multi_assign, s, nullptr);
    }
    break;
  }
  return 0;
}

int AsyncHassHandler::TimeoutCallback(CURLM *multi, int timeoutMs,
                                      CurlMultiContext *mc) {
  if (timeoutMs < 0) {
    mc->pSched_->unscheduleDelayedTask(mc->token_);
  } else {
    if (timeoutMs == 0) {
      timeoutMs = 1;
    }
    mc->pSched_->scheduleDelayedTask(timeoutMs * 1000,
                                     AsyncHassHandler::TimeoutHandlerProc, mc);
  }
  return 0;
}

void AsyncHassHandler::CheckMultiInfo(CurlSocketContext &csc) {
  CURLMsg *message{nullptr};
  auto *mc = csc.pMultiContext;

  int pending{0};

  auto &wCurlMulti = mc->wCurlMulti_;
  while ((message = curl_multi_info_read(wCurlMulti.pCurl_, &pending))) {
    switch (message->msg) {
    case CURLMSG_DONE: {
      CURL *easy = message->easy_handle;
      int ctxId{-1};
      curl_easy_getinfo(easy, CURLINFO_PRIVATE, &ctxId);
      if (auto ec = mc->pHandler->ctxs_[ctxId]) {
        std::cerr << std::string_view(ec->buf) << "\n";

        // cleanup
        wCurlMulti(curl_multi_remove_handle, ec->wCurl.pCurl_);
        mc->pHandler->ctxs_.erase(ctxId);
      }
      break;
    }
    default:
      std::cerr << "CURLMSG default\n";
      break;
    }
  }
}

void AsyncHassHandler::BackgroundHandlerProc(void *clientData, int mask) {
  int flags{0};
  if (mask & SOCKET_READABLE) {
    flags |= CURL_CSELECT_IN;
  }
  if (mask & SOCKET_WRITABLE) {
    flags |= CURL_CSELECT_OUT;
  }
  CurlSocketContext *csc = static_cast<CurlSocketContext *>(clientData);
  int runningHandles{0};
  csc->pMultiContext->wCurlMulti_(curl_multi_socket_action, csc->sockfd, flags,
                                  &runningHandles);
}

void AsyncHassHandler::TimeoutHandlerProc(void *curlMultiContext_clientData) {
  if (curlMultiContext_clientData) {
    CurlMultiContext *mc =
        static_cast<CurlMultiContext *>(curlMultiContext_clientData);
    int runningHandles{-1};
    mc->wCurlMulti_(curl_multi_socket_action, CURL_SOCKET_TIMEOUT, 0,
                    &runningHandles);
  }
}

void AsyncHassHandler::DebounceUpdateProc(void *asyncHassHandler_clientData) {
  if (asyncHassHandler_clientData) {
    AsyncHassHandler *ahh =
        static_cast<AsyncHassHandler *>(asyncHassHandler_clientData);
    LOGGER->debug("Restoring update ability to {}", ahh->entityId);
    ahh->allowUpdate_ = true;
  }
}

AsyncHassHandler::AsyncHassHandler(const boost::url &url,
                                   const std::string &token,
                                   const std::string &entityId)
    : BaseHassHandler(url, token, entityId) {
  curlMultiCtx_.pHandler = this;
}

void AsyncHassHandler::Register(TaskScheduler *pSched) {
  if (!pSched) {
    throw std::invalid_argument("Usage Environment was null");
  }

  curlMultiCtx_.pSched_ = pSched;

  curlMultiCtx_.wCurlMulti_(curl_multi_setopt, CURLMOPT_SOCKETFUNCTION,
                            SocketCallback);
  curlMultiCtx_.wCurlMulti_(curl_multi_setopt, CURLMOPT_SOCKETDATA,
                            &curlMultiCtx_);
  curlMultiCtx_.wCurlMulti_(curl_multi_setopt, CURLMOPT_TIMERFUNCTION,
                            TimeoutCallback);
  curlMultiCtx_.wCurlMulti_(curl_multi_setopt, CURLMOPT_TIMERDATA,
                            &curlMultiCtx_);

  int runningHandles{0};
  curlMultiCtx_.wCurlMulti_(curl_multi_socket_action, CURL_SOCKET_TIMEOUT, 0,
                            &runningHandles);
}

void AsyncHassHandler::UpdateState(std::string_view state,
                                   const json &attributes) {
  static size_t id{0};

  const bool stateChanging = nextState_ != currentState_;
  if (allowUpdate_ && stateChanging) {
    // insert and  get a reference to this
    // not thread safe
    auto pCtx = std::make_shared<CurlEasyContext>(curlMultiCtx_.pSched_);
    pCtx->wCurl(curl_easy_setopt, CURLOPT_WRITEDATA, &pCtx->buf);
    pCtx->wCurl(curl_easy_setopt, CURLOPT_WRITEFUNCTION,
                util::FillBufferCallback);
    pCtx->wCurl(curl_easy_setopt, CURLOPT_PRIVATE, id);
    pCtx->wCurl(curl_easy_setopt, CURLOPT_URL, GetUrl().c_str());
    pCtx->wCurl(curl_easy_setopt, CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
    pCtx->wCurl(curl_easy_setopt, CURLOPT_XOAUTH2_BEARER, GetToken().c_str());
    curlMultiCtx_.wCurlMulti_(curl_multi_add_handle, pCtx->wCurl.pCurl_);
    ctxs_[id++] = pCtx;
    allowUpdate_ = false;
    curlMultiCtx_.pSched_->scheduleDelayedTask(
        std::chrono::duration_cast<std::chrono::microseconds>(debounceTime)
            .count(),
        DebounceUpdateProc, this);
  }
}

} // namespace home_assistant