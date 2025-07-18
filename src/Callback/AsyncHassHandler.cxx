#include "Logger.h"
#include "WindowsWrapper.h"

#include "Callback/AsyncHassHandler.h"

#include <ranges>
#include <string_view>

#include <UsageEnvironment.hh>

#include "Callback/Json.h"
#include "Util/BufferOperations.h"
#include "Util/Tools.h"

using namespace std::string_literals;
using namespace std::string_view_literals;

namespace {
static size_t contextId{1};
} // namespace

namespace callback {

AsyncHassHandler::AsyncHassHandler(TaskScheduler *pSched, const boost::url &url,
                                   const std::string &token,
                                   const std::string &entityId)
    : BaseHassHandler(url, token, entityId), AsyncDebouncer(pSched),
      pSched_{pSched} {}

AsyncHassHandler::~AsyncHassHandler() noexcept {
  for (const auto &socketCtx : socketCtxs_ | std::views::values) {
    pSched_->disableBackgroundHandling(socketCtx->sockfd);
    wCurlMulti_(curl_multi_assign, socketCtx->sockfd, nullptr);
  }
}

void AsyncHassHandler::Register() {
  // startup CURL and get the initial state
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
  if (UpdateAllowed() && stateChanging) {
    // insert and  get a reference to this
    // not thread safe
    auto pCtx = easyCtxs_[contextId++] = std::make_shared<_CurlEasyContext>();

    PreparePostRequest(pCtx->wCurl, pCtx->writeData, pCtx->readData);

    wCurlMulti_(curl_multi_add_handle, pCtx->wCurl.pCurl_);
    pCtx->wCurl(curl_easy_setopt, CURLOPT_PRIVATE, contextId);

    // debounce
    Debounce(debounceTime);
  }
}

void AsyncHassHandler::GetInitialState() {
  auto pCtx = easyCtxs_[contextId++] = std::make_shared<_CurlEasyContext>();

  PrepareGetRequest(pCtx->wCurl, pCtx->writeData);

  wCurlMulti_(curl_multi_add_handle, pCtx->wCurl.pCurl_);
  pCtx->wCurl(curl_easy_setopt, CURLOPT_PRIVATE, contextId);
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
            HandleGetResponse(pCtx->wCurl, pCtx->writeData);
          } else if (util::NoCaseCmp(effectiveMethod, "POST")) {
            HandlePostResponse(pCtx->wCurl, pCtx->writeData);
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
      LOGGER->warn("CURLMSG default");
      break;
    }
  }
}

int AsyncHassHandler::SocketCallback(CURL *easy, curl_socket_t s, int action,
                                     AsyncHassHandler *asyncHassHandler,
                                     _CurlSocketContext *curlSocketContext) {
  if (auto pAhh = asyncHassHandler->weak_from_this().lock()) {
    std::shared_ptr<_CurlSocketContext> pCtx;
    if (curlSocketContext) {
      pCtx = static_cast<_CurlSocketContext *>(curlSocketContext)
                 ->shared_from_this();
    };
    switch (action) {
    case CURL_POLL_IN:
    case CURL_POLL_OUT:
    case CURL_POLL_INOUT: {
      if (!pCtx) {
        pCtx = std::make_shared<_CurlSocketContext>();
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
          asyncHassHandler->timeoutTaskToken_);
    } else {
      if (timeoutMs == 0) {
        timeoutMs = 1;
      }
      asyncHassHandler->timeoutTaskToken_ =
          asyncHassHandler->pSched_->scheduleDelayedTask(
              timeoutMs * 1000, AsyncHassHandler::TimeoutHandlerProc,
              asyncHassHandler);
    }
  }
  return 0;
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
    auto csc = static_cast<_CurlSocketContext *>(curlSocketContext_clientData)
                   ->shared_from_this();
    if (auto pAhh = csc->pHandler.lock()) {
      int runningHandles{0};
      pAhh->wCurlMulti_(curl_multi_socket_action, csc->sockfd, flags,
                        &runningHandles);
      pAhh->CheckMultiInfo();
    }
  }
}

void AsyncHassHandler::TimeoutHandlerProc(void *asyncHassHandler_clientData) {
  if (asyncHassHandler_clientData) {
    auto pAhh = static_cast<AsyncHassHandler *>(asyncHassHandler_clientData);
    int runningHandles{-1};
    pAhh->wCurlMulti_(curl_multi_socket_action, CURL_SOCKET_TIMEOUT, 0,
                      &runningHandles);
    pAhh->CheckMultiInfo();
  }
}

} // namespace callback