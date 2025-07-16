#include "Logger.h"
#include "WindowsWrapper.h"

#include "Callback/AsyncFileSave.h"

#include "Util/BufferOperations.h"
#include "Util/Tools.h"

#include <bit>
#include <ranges>

#if __linux__
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#define IO_SIGNAL SIGUSR1
#endif

namespace {
static size_t contextId{1};

#if _WIN32
std::string GetErrorMessage(DWORD dwErrorCode) {
  LPVOID lpMsgBuf{nullptr};

  FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                    FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, dwErrorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPTSTR)&lpMsgBuf, 0, NULL);

  if (lpMsgBuf) {
    return std::format(TEXT("Error ({}): {}"), dwErrorCode, (LPTSTR)lpMsgBuf);
    LocalFree(lpMsgBuf);
  } else {
    return "Error, unable to receive error message";
  }
}
#endif

} // namespace

namespace callback {

AsyncFileSave::AsyncFileSave(const std::filesystem::path &dstPath,
                             const boost::url &url, const std::string &user,
                             const std::string &password)
    : dstPath_{dstPath}, url_{url} {
  if (!user.empty()) {
    url_.set_user(user);
  }
  if (!password.empty()) {
    url_.set_password(password);
  }
}

AsyncFileSave::~AsyncFileSave() noexcept {
  for (const auto &socketCtx : socketCtxs_ | std::views::values) {
    pSched_->disableBackgroundHandling(socketCtx->sockfd);
    wCurlMulti_(curl_multi_assign, socketCtx->sockfd, nullptr);
  }
}

void AsyncFileSave::Register(TaskScheduler *pSched) {
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

#ifdef __linux__
  InstallHandlers();
#endif
}

void AsyncFileSave::SaveFileAtEndpoint() {
  // prepare a context
  if (!pSched_) {
    throw std::invalid_argument(
        "Must register first with a Usage Environment Scheduler");
  }

  // format a filename based on the current time, or using a counter

  auto dst = dstPath_;
  if (std::filesystem::is_directory(dst)) {
    const auto now = std::chrono::system_clock::now();
    const auto fileName = std::format("{:%Y-%m-%d_%H-%M-%S}.jpg", now);
    dst /= fileName;
  }

  auto pCtx = easyCtxs_[contextId] = std::make_shared<_CurlEasyContext>();

  try {
#if _WIN32
    pCtx->writeData.hFile =
        CreateFile(dst.string().c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                   FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
    pCtx->writeData.overlapped.hEvent = static_cast<HANDLE>(pCtx.get());
#elif __linux__
    pCtx->writeData._aiocb.aio_fildes = open(
        dst.string().c_str(), O_CREAT | O_WRONLY | O_ASYNC | O_TRUNC, 0666);
    if (pCtx->writeData._aiocb.aio_fildes == -1) {
      throw std::runtime_error(strerror(errno));
    }
    pCtx->writeData._aiocb.aio_reqprio = 0;
    pCtx->writeData._aiocb.aio_offset = 0;
    pCtx->writeData._aiocb.aio_sigevent.sigev_notify = SIGEV_SIGNAL;
    pCtx->writeData._aiocb.aio_sigevent.sigev_signo = IO_SIGNAL;
    pCtx->writeData._aiocb.aio_sigevent.sigev_value.sival_ptr = pCtx.get();
#endif
    pCtx->writeData.dstPath = dst;
    pCtx->pHandler = this->weak_from_this();
    pCtx->contextId = contextId;

    pCtx->wCurl(curl_easy_setopt, CURLOPT_WRITEFUNCTION,
                AsyncFileSave::WriteFunction);
    pCtx->wCurl(curl_easy_setopt, CURLOPT_PRIVATE, contextId);

    pCtx->wCurl(curl_easy_setopt, CURLOPT_URL, url_.c_str());
    pCtx->wCurl(curl_easy_setopt, CURLOPT_HTTPGET, 1L);
    pCtx->wCurl(curl_easy_setopt, CURLOPT_WRITEDATA, &pCtx->writeData);

    wCurlMulti_(curl_multi_add_handle, pCtx->wCurl.pCurl_);
    pCtx->wCurl(curl_easy_setopt, CURLOPT_PRIVATE, contextId);

    ++contextId;
    return;
  } catch (const std::exception &e) {
    LOGGER->error(e.what());
  } catch (...) {
    LOGGER->error("Unknown error setting up Async File Save");
  }
#if _WIN32
  CloseHandle(pCtx->writeData.hFile);
#elif __linux__
  close(pCtx->writeData._aiocb.aio_fildes);
#endif
  easyCtxs_.erase(contextId);
}

const boost::circular_buffer<std::filesystem::path> &
AsyncFileSave::GetSavedFilePaths() const {
  return savedFilePaths_;
}

void AsyncFileSave::SetLimitSavedFilePaths(size_t limit) {
  savedFilePaths_.set_capacity(limit);
}

void AsyncFileSave::CheckMultiInfo() {
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
#if _WIN32
            DWORD bytesTransferred{0};
            GetOverlappedResult(pCtx->writeData.hFile,
                                &pCtx->writeData.overlapped, &bytesTransferred,
                                TRUE);
#endif
          }
        } catch (const std::exception &e) {
          LOGGER->error(e.what());
        }

        // cleanup
        wCurlMulti(curl_multi_remove_handle, pCtx->wCurl.pCurl_);
      }
      break;
    }
    default:
      LOGGER->warn("CURLMSG default");
      break;
    }
  }
}

int AsyncFileSave::SocketCallback(CURL *easy, curl_socket_t s, int action,
                                  AsyncFileSave *asyncFileSave,
                                  _CurlSocketContext *curlSocketContext) {
  if (auto pAfs = asyncFileSave->weak_from_this().lock()) {
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
        pCtx->pHandler = pAfs;
        pAfs->socketCtxs_[s] = pCtx; // save to the map for pointer preservation
      }

      pAfs->wCurlMulti_(curl_multi_assign, s, pCtx.get());

      int flags{0};
      flags |= (action != CURL_POLL_IN) ? SOCKET_WRITABLE : 0;
      flags |= (action != CURL_POLL_OUT) ? SOCKET_READABLE : 0;
      pAfs->pSched_->setBackgroundHandling(
          s, flags, AsyncFileSave::BackgroundHandlerProc, pCtx.get());
    } break;
    case CURL_POLL_REMOVE:
      pAfs->pSched_->disableBackgroundHandling(s);
      pAfs->wCurlMulti_(curl_multi_assign, s, nullptr);
      pAfs->socketCtxs_.erase(s);
      break;
    }
  }
  return 0;
}

int AsyncFileSave::TimeoutCallback(CURLM *multi, int timeoutMs,
                                   AsyncFileSave *asyncFileSave) {
  if (asyncFileSave) {
    if (timeoutMs < 0) {
      asyncFileSave->pSched_->unscheduleDelayedTask(asyncFileSave->token_);
    } else {
      if (timeoutMs == 0) {
        timeoutMs = 1;
      }
      asyncFileSave->token_ = asyncFileSave->pSched_->scheduleDelayedTask(
          timeoutMs * 1000, AsyncFileSave::TimeoutHandlerProc, asyncFileSave);
    }
  }
  return 0;
}

void AsyncFileSave::BackgroundHandlerProc(void *curlSocketContext_clientData,
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
    if (auto pAfs = csc->pHandler.lock()) {
      int runningHandles{0};
      pAfs->wCurlMulti_(curl_multi_socket_action, csc->sockfd, flags,
                        &runningHandles);
      pAfs->CheckMultiInfo();
    }
  }
}

void AsyncFileSave::TimeoutHandlerProc(void *asyncFileSave_clientData) {
  if (asyncFileSave_clientData) {
    auto pAfs = static_cast<AsyncFileSave *>(asyncFileSave_clientData);
    int runningHandles{-1};
    pAfs->wCurlMulti_(curl_multi_socket_action, CURL_SOCKET_TIMEOUT, 0,
                      &runningHandles);
    pAfs->CheckMultiInfo();
  }
}

size_t AsyncFileSave::WriteFunction(char *contents, size_t sz, size_t nmemb,
                                    void *pUserData) {
  if (pUserData) {
    size_t realsize = sz * nmemb;
#if _WIN32
    auto *pWin32Overlapped = std::bit_cast<Win32Overlapped *>(pUserData);
    if (!WriteFileEx(pWin32Overlapped->hFile, contents, realsize,
                     &pWin32Overlapped->overlapped, FileIOCompletionRoutine)) {
      LOGGER->error(GetErrorMessage(GetLastError()));
    }
#elif __linux__
    auto *pLinuxAioFile = static_cast<LinuxAioFile *>(pUserData);

    pLinuxAioFile->buf.resize(realsize, '\0');
    memcpy(pLinuxAioFile->buf.data(), contents, realsize);
    pLinuxAioFile->_aiocb.aio_buf = pLinuxAioFile->buf.data();
    pLinuxAioFile->_aiocb.aio_nbytes = pLinuxAioFile->buf.size();

    if (aio_write(&pLinuxAioFile->_aiocb) == -1) {
      LOGGER->error(strerror(errno));
    }
#endif
    return realsize;
  }
  return 0;
}

#if _WIN32

VOID CALLBACK AsyncFileSave::FileIOCompletionRoutine(
    __in DWORD dwErrorCode, __in DWORD dwNumberOfBytesTransferred,
    __in LPOVERLAPPED lpOverlapped) {
  auto pCtx = static_cast<_CurlEasyContext *>(lpOverlapped->hEvent);
  if (!CloseHandle(pCtx->writeData.hFile)) {
    LOGGER->warn("Failed to close file");
  } else {
    LOGGER->info("Closed handle to {}", pCtx->writeData.hFile);
  }
  if (auto pHandler = pCtx->pHandler.lock()) {
    pHandler->savedFilePaths_.push_back(std::move(pCtx->writeData.dstPath));
    pHandler->easyCtxs_.erase(pCtx->contextId);
  }
}

#elif __linux__

void AsyncFileSave::AioSigHandler(int sig, siginfo_t *si, void *) {
  if (si->si_code == SI_ASYNCIO) {
    auto *pCtx = static_cast<_CurlEasyContext *>(si->si_value.sival_ptr);
    const ssize_t bytesWritten = aio_return(&pCtx->writeData._aiocb);
    if (bytesWritten == -1) {
      LOGGER->error(strerror(errno));
      close(pCtx->writeData._aiocb.aio_fildes);
    } else if (bytesWritten == pCtx->writeData.buf.size()) {
      LOGGER->info("File IO complete {}", pCtx->writeData.dstPath.string());
      close(pCtx->writeData._aiocb.aio_fildes);

      if (auto pHandler = pCtx->pHandler.lock()) {
        pHandler->pSched_->scheduleDelayedTask(
            0,
            [](void *clientData) {
              if (!clientData) {
                return;
              }
              auto *pCtx = static_cast<_CurlEasyContext *>(clientData);
              if (auto pHandler = pCtx->pHandler.lock()) {
                pHandler->savedFilePaths_.push_back(
                    std::move(pCtx->writeData.dstPath));
                pHandler->easyCtxs_.erase(pCtx->contextId);
              }
            },
            pCtx);
      }
    } else {
      LOGGER->info("Filo IO incomplete {}, {} bytes written",
                   pCtx->writeData.dstPath.string(), bytesWritten);
    }
  }
}

void AsyncFileSave::InstallHandlers() {
  struct sigaction sa{};
  sigaction(IO_SIGNAL, nullptr, &sa);
  if (sa.sa_sigaction == AioSigHandler) {
    return;
  }
  memset(&sa, 0, sizeof(sa));
  sa.sa_flags = SA_RESTART | SA_SIGINFO;
  sa.sa_sigaction = AioSigHandler;
  if (sigaction(IO_SIGNAL, &sa, nullptr) == -1) {
    throw std::runtime_error(strerror(errno));
  }
}

#endif

} // namespace callback