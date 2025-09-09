#include "Logger.h"
#include "WindowsWrapper.h"

#include "Callback/AsyncFileSave.h"

#include "Util/BufferOperations.h"
#include "Util/Tools.h"

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
    return std::format(TEXT("Error ({}): {}"), dwErrorCode,
                       static_cast<LPTSTR>(lpMsgBuf));
    LocalFree(lpMsgBuf);
  } else {
    return "Error, unable to receive error message";
  }
}

void LogLastError() {
  const auto error = GetLastError();
  LOGGER->error("Win32 Error ({}): {}", error, GetErrorMessage(error));
}

#elif __linux__

void LogLastError() {
  const auto error = errno;
  LOGGER->error("Linux Error ({}): {}", error, strerror(error));
}

#endif

} // namespace

namespace callback {

AsyncFileSave::AsyncFileSave(std::shared_ptr<TaskScheduler> pSched,
                             const std::filesystem::path &dstPath,
                             const boost::url &url, const std::string &user,
                             const std::string &password)
    : AsyncDebouncer(pSched), pSched_{pSched}, dstPath_{dstPath}, url_{url},
      user_{user}, password_{password} {
  if (!std::filesystem::exists(dstPath_)) {
    std::filesystem::create_directories(dstPath_);
  }
  spareBuf_.reserve(defaultJpgBufferSize);
  savedFilePaths_.set_capacity(defaultSavedFilePathsSize);
}

AsyncFileSave::~AsyncFileSave() noexcept {
  for (const auto &socketCtx : socketCtxs_ | std::views::values) {
    pSched_->disableBackgroundHandling(socketCtx->sockfd);
    wCurlMulti_(curl_multi_assign, socketCtx->sockfd, nullptr);
  }
}

void AsyncFileSave::Register() {
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

void AsyncFileSave::SaveFileAtEndpoint(const std::filesystem::path &_dst) {

  // format a filename based on the current time
  auto dst = dstPath_;
  if (std::filesystem::is_directory(dstPath_) && _dst.empty()) {
    const auto now = std::chrono::system_clock::now();
    const auto fileName = std::format("{:%Y-%m-%d_%H-%M-%S}.jpg", now);
    dst /= fileName;
  } else if (!_dst.has_parent_path()) {
    // if the path is not absolute, append it to the download directory
    dst /= _dst;
  } else {
    // if the path is absolute, use it as is
    dst = _dst;
  }

  auto pCtx = std::make_shared<_CurlEasyContext>();

  try {
#if _WIN32
    pCtx->writeData.hFile =
        CreateFile(dst.string().c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                   FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
    ZeroMemory(&pCtx->writeData.overlapped, sizeof(pCtx->writeData.overlapped));
    pCtx->writeData.overlapped.Offset = 0xFFFFFFFF;
    pCtx->writeData.overlapped.OffsetHigh = 0xFFFFFFFF;
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
    pCtx->writeData.dstPath = std::move(dst);
    pCtx->writeData.buf = std::move(spareBuf_);
    pCtx->writeData.buf.clear();
    pCtx->pHandler = this->weak_from_this();
    pCtx->contextId = contextId;

    pCtx->wCurl(curl_easy_setopt, CURLOPT_WRITEFUNCTION,
                util::FillBufferCallback);
    pCtx->wCurl(curl_easy_setopt, CURLOPT_PRIVATE, contextId);

    pCtx->wCurl(curl_easy_setopt, CURLOPT_URL, url_.c_str());
    pCtx->wCurl(curl_easy_setopt, CURLOPT_HTTPGET, 1L);
    pCtx->wCurl(curl_easy_setopt, CURLOPT_WRITEDATA, &pCtx->writeData.buf);

    if (!user_.empty() && !password_.empty()) {
      pCtx->wCurl(curl_easy_setopt, CURLOPT_HTTPAUTH, CURLAUTH_DIGEST);
      pCtx->wCurl(curl_easy_setopt, CURLOPT_USERNAME, user_.c_str());
      pCtx->wCurl(curl_easy_setopt, CURLOPT_PASSWORD, password_.c_str());
    }

    wCurlMulti_(curl_multi_add_handle, pCtx->wCurl.pCurl_);
    pCtx->wCurl(curl_easy_setopt, CURLOPT_PRIVATE, contextId);

    easyCtxs_[contextId] = pCtx;
    ++contextId;
  } catch (const std::exception &e) {
    LOGGER->error(e.what());
  } catch (...) {
    LOGGER->error("Unknown error setting up Async File Save");
  }
}

void AsyncFileSave::operator()(detector::Payload data) {

  static decltype(data.rois) lastRois = {};

  const bool risingEdge = lastRois.empty() && !data.rois.empty();

  if (UpdateAllowed() && risingEdge) {
    SaveFileAtEndpoint();
    Debounce(debounceTime);
  }

  const bool reschedule = !UpdateAllowed() && !data.rois.empty();
  if (reschedule) {
    Debounce(debounceTime);
  }
}

const boost::circular_buffer<std::filesystem::path> &
AsyncFileSave::GetSavedFilePaths() const {
  return savedFilePaths_;
}

void AsyncFileSave::SetLimitSavedFilePaths(size_t limit) {
  while (savedFilePaths_.size() > limit) {
    if (!std::filesystem::remove(savedFilePaths_.front())) {
      LOGGER->warn("Failed to remove file: {}", savedFilePaths_.front());
    } else {
      LOGGER->info("Removed file: {}", savedFilePaths_.front());
    }
    savedFilePaths_.pop_front();
  }
  savedFilePaths_.set_capacity(limit);
}

#if _WIN32

AsyncFileSave::Win32Overlapped::~Win32Overlapped() noexcept {
  if (hFile != INVALID_HANDLE_VALUE && !CloseHandle(hFile)) {
    const auto error = GetLastError();
    LOGGER->error("Failed to close {} with error ({}): {}", dstPath, error,
                  GetErrorMessage(error));
  }
}

#elif __linux__

AsyncFileSave::LinuxAioFile::~LinuxAioFile() {
  if (close(_aiocb.aio_fildes) == -1) {
    LOGGER->error("Failed to close {} with error ({}): {}", dstPath, errno,
                  strerror(errno));
  }
}

#endif

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
          int responseCode{0};
          curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &responseCode);

          switch (responseCode) {
          case 200:
          case 201:
            if (util::NoCaseCmp(effectiveMethod, "GET")) {
#if _WIN32
              const auto res = WriteFileEx(
                  pCtx->writeData.hFile, pCtx->writeData.buf.data(),
                  pCtx->writeData.buf.size(), &pCtx->writeData.overlapped,
                  AsyncFileSave::FileIOCompletionRoutine);
              if (res == ERROR) {
                LogLastError();
                std::filesystem::remove(pCtx->writeData.dstPath);
              }
#elif __linux__
              pCtx->writeData._aiocb.aio_buf = pCtx->writeData.buf.data();
              pCtx->writeData._aiocb.aio_nbytes = pCtx->writeData.buf.size();
              const auto res = aio_write(&pCtx->writeData._aiocb);
              if (res == -1) {
                LogLastError();
                std::filesystem::remove(pCtx->writeData.dstPath);
              }
#endif
            }
            break;
          default:
            LOGGER->info("CURL Request failed with response code {}",
                         responseCode);
            std::filesystem::remove(pCtx->writeData.dstPath);
            wCurlMulti(curl_multi_remove_handle, pCtx->wCurl.pCurl_);
            RemoveContext(pCtx.get());
            return;
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

#if _WIN32

VOID CALLBACK AsyncFileSave::FileIOCompletionRoutine(
    __in DWORD dwErrorCode, __in DWORD dwNumberOfBytesTransferred,
    __in LPOVERLAPPED lpOverlapped) {
  if (dwErrorCode != NOERROR) {
    LOGGER->error(GetErrorMessage(dwErrorCode));
  }
  if (lpOverlapped->hEvent) {
    auto pCtx = static_cast<_CurlEasyContext *>(lpOverlapped->hEvent);
    if (dwNumberOfBytesTransferred != pCtx->writeData.buf.size()) {
      LOGGER->error("File IO incomplete for {}, {} bytes written of {}",
                    pCtx->writeData.dstPath.string(),
                    dwNumberOfBytesTransferred, pCtx->writeData.buf.size());
    }
    RemoveContext(pCtx);
  }
}

#elif __linux__

void AsyncFileSave::AioSigHandler(int sig, siginfo_t *si, void *) {
  if (si->si_code == SI_ASYNCIO) {
    auto *pCtx = static_cast<_CurlEasyContext *>(si->si_value.sival_ptr);
    const ssize_t bytesWritten = aio_return(&pCtx->writeData._aiocb);
    if (bytesWritten == -1) {
      LOGGER->error(strerror(errno));
    } else if (bytesWritten == pCtx->writeData.buf.size()) {
      LOGGER->info("File IO complete {}", pCtx->writeData.dstPath);
    } else {
      LOGGER->info("Filo IO incomplete {}, {} bytes written",
                   pCtx->writeData.dstPath, bytesWritten);
      return; // do not remove context yet
    }
    if (auto pHandler = pCtx->pHandler.lock()) {
      pHandler->pSched_->scheduleDelayedTask(
          0,
          [](void *clientData) {
            if (!clientData) {
              return;
            }
            RemoveContext(static_cast<_CurlEasyContext *>(clientData));
          },
          pCtx);
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

void AsyncFileSave::RemoveContext(_CurlEasyContext *pCtx) {
  if (!pCtx) {
    return; // no-op
  }
  if (auto pHandler = pCtx->pHandler.lock()) {
    if (pHandler->savedFilePaths_.full()) {
      const auto &oldFile = pHandler->savedFilePaths_.front();
      if (std::filesystem::remove(oldFile)) {
        LOGGER->info("Removed {}, maximum file buffer size reached ({})",
                     oldFile, pHandler->savedFilePaths_.capacity());
      } else {
        LOGGER->warn(
            "Failed to remove {}, maximum saved images reached ({}), but "
            "old data has not been deleted, the disk may begin to fill",
            oldFile, pHandler->savedFilePaths_.capacity());
      }
    }
    pHandler->savedFilePaths_.push_back(pCtx->writeData.dstPath);

    // Avoid reallocating a buffer, stash it in a node with a max key
    pHandler->spareBuf_.swap(pCtx->writeData.buf);
    pHandler->easyCtxs_.erase(pCtx->contextId);
  }
}

} // namespace callback