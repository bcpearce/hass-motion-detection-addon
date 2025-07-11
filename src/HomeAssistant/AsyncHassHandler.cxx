#include "Logger.h"
#include "WindowsWrapper.h"

#include "HomeAssistant/AsyncHassHandler.h"

#include <iostream>
#include <ranges>
#include <string_view>

#include <UsageEnvironment.hh>

#include "HomeAssistant/Json.h"
#include "Util/BufferOperations.h"
#include "Util/CurlWrapper.h"
#include "Util/Tools.h"

using namespace std::string_literals;
using namespace std::string_view_literals;

namespace {
static size_t contextId{1};
} // namespace

namespace home_assistant {
int AsyncHassHandler::SocketCallback(CURL *easy, curl_socket_t s, int action,
                                     AsyncHassHandler *asyncHassHandler,
                                     CurlSocketContext *curlSocketContext) {
  if (auto pAhh = asyncHassHandler->weak_from_this().lock()) {
    std::shared_ptr<CurlSocketContext> pCtx;
    if (curlSocketContext) {
      pCtx = static_cast<CurlSocketContext *>(curlSocketContext)
                 ->shared_from_this();
    };
    switch (action) {
    case CURL_POLL_IN:
    case CURL_POLL_OUT:
    case CURL_POLL_INOUT: {
      if (!pCtx) {
        pCtx = std::make_shared<CurlSocketContext>();
        pCtx->sockfd = s;
        pCtx->pHandler = pAhh;
        pAhh->socketCtxs_[s] = pCtx; // save to the map for pointer preservation
      }

      pAhh->wCurlMulti_(curl_multi_assign, s, pCtx.get());

      int flags{0};
      flags |= (action != CURL_POLL_IN) ? SOCKET_WRITABLE : 0;
      flags |= (action != CURL_POLL_OUT) ? SOCKET_READABLE : 0;
      pAhh->pSched_->setBackgroundHandling(
          s, flags, AsyncHassHandler::BackgroundHandlerProc, pCtx.get());
    } break;
    case CURL_POLL_REMOVE:
      pAhh->pSched_->disableBackgroundHandling(s);
      pAhh->wCurlMulti_(curl_multi_assign, s, nullptr);
      pAhh->socketCtxs_.erase(s);
      break;
    }
  }
  return 0;
}

int AsyncHassHandler::TimeoutCallback(CURLM *multi, int timeoutMs,
                                      AsyncHassHandler *asyncHassHandler) {
  if (asyncHassHandler) {
    if (timeoutMs < 0) {
      asyncHassHandler->pSched_->unscheduleDelayedTask(
          asyncHassHandler->token_);
    } else {
      if (timeoutMs == 0) {
        timeoutMs = 1;
      }
      asyncHassHandler->token_ = asyncHassHandler->pSched_->scheduleDelayedTask(
          timeoutMs * 1000, AsyncHassHandler::TimeoutHandlerProc,
          asyncHassHandler);
    }
  }
  return 0;
}

void AsyncHassHandler::CheckMultiInfo() {
  CURLMsg *message{nullptr};

  int pending{0};

  auto &wCurlMulti = wCurlMulti_;
  while ((message = curl_multi_info_read(wCurlMulti.pCurl_, &pending))) {
    switch (message->msg) {
    case CURLMSG_DONE: {
      size_t ctxId{0};
      CURL *easy = message->easy_handle;

      curl_easy_getinfo(easy, CURLINFO_PRIVATE, &ctxId);
      auto it = easyCtxs_.find(ctxId);
      if (it != easyCtxs_.end()) {
        auto &pCtx = it->second;
        try {
          char *effectiveMethod{nullptr};
          curl_easy_getinfo(easy, CURLINFO_EFFECTIVE_METHOD, &effectiveMethod);
          if (util::NoCaseCmp(effectiveMethod, "GET")) {
            HandleGetResponse(pCtx->wCurl, pCtx->inBuf);
          } else if (util::NoCaseCmp(effectiveMethod, "POST")) {
            HandlePostResponse(pCtx->wCurl, pCtx->inBuf);
          }
        } catch (const std::exception &e) {
          LOGGER->error(e.what());
        }

        // cleanup
        wCurlMulti(curl_multi_remove_handle, pCtx->wCurl.pCurl_);
        easyCtxs_.erase(it);
      }
      break;
    }
    default:
      std::cerr << "CURLMSG default\n";
      break;
    }
  }
}

void AsyncHassHandler::BackgroundHandlerProc(void *curlSocketContext_clientData,
                                             int mask) {
  if (curlSocketContext_clientData) {
    int flags{0};
    if (mask & SOCKET_READABLE) {
      flags |= CURL_CSELECT_IN;
    }
    if (mask & SOCKET_WRITABLE) {
      flags |= CURL_CSELECT_OUT;
    }
    auto csc = static_cast<CurlSocketContext *>(curlSocketContext_clientData)
                   ->shared_from_this();
    if (auto ahh = csc->pHandler.lock()) {
      int runningHandles{0};
      ahh->wCurlMulti_(curl_multi_socket_action, csc->sockfd, flags,
                       &runningHandles);
      ahh->CheckMultiInfo();
    }
  }
}

void AsyncHassHandler::TimeoutHandlerProc(void *asyncHassHandler_clientData) {
  if (asyncHassHandler_clientData) {
    auto ahh = static_cast<AsyncHassHandler *>(asyncHassHandler_clientData);
    int runningHandles{-1};
    ahh->wCurlMulti_(curl_multi_socket_action, CURL_SOCKET_TIMEOUT, 0,
                     &runningHandles);
    ahh->CheckMultiInfo();
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
    : BaseHassHandler(url, token, entityId) {}

void AsyncHassHandler::Register(TaskScheduler *pSched) {
  if (!pSched) {
    throw std::invalid_argument("Usage Environment was null");
  }

  pSched_ = pSched;
  wCurlMulti_(curl_multi_setopt, CURLMOPT_SOCKETFUNCTION, SocketCallback);
  wCurlMulti_(curl_multi_setopt, CURLMOPT_SOCKETDATA, this);
  wCurlMulti_(curl_multi_setopt, CURLMOPT_TIMERFUNCTION, TimeoutCallback);
  wCurlMulti_(curl_multi_setopt, CURLMOPT_TIMERDATA, this);

  int runningHandles{0};
  wCurlMulti_(curl_multi_socket_action, CURL_SOCKET_TIMEOUT, 0,
              &runningHandles);

  GetInitialState();
}

void AsyncHassHandler::UpdateState_Impl(std::string_view state,
                                        const json &attributes) {
  UpdateStateInternal(state, attributes);

  const bool stateChanging = IsStateChanging();
  if (allowUpdate_ && stateChanging) {
    // insert and  get a reference to this
    // not thread safe
    auto pCtx = std::make_shared<CurlEasyContext>();

    pCtx->wCurl(curl_easy_setopt, CURLOPT_PRIVATE, contextId);
    PreparePostRequest(pCtx->wCurl, pCtx->inBuf, pCtx->outBuf);

    wCurlMulti_(curl_multi_add_handle, pCtx->wCurl.pCurl_);
    easyCtxs_[contextId++] = pCtx;

    // debounce
    allowUpdate_ = false;
    pSched_->scheduleDelayedTask(
        std::chrono::duration_cast<std::chrono::microseconds>(debounceTime)
            .count(),
        DebounceUpdateProc, this);
  }
}

void AsyncHassHandler::GetInitialState() {
  auto pCtx = std::make_shared<CurlEasyContext>();

  pCtx->wCurl(curl_easy_setopt, CURLOPT_PRIVATE, contextId);
  PrepareGetRequest(pCtx->wCurl, pCtx->inBuf);

  wCurlMulti_(curl_multi_add_handle, pCtx->wCurl.pCurl_);
  easyCtxs_[contextId++] = pCtx;
}

} // namespace home_assistant