#pragma once

#include "WindowsWrapper.h"

#include "Callback/Context.h"

#include <filesystem>
#include <memory>
#include <string_view>
#include <unordered_map>

#include <UsageEnvironment.hh>
#include <boost/url.hpp>

#include "Util/CurlMultiWrapper.h"
#include "Util/CurlWrapper.h"

#if __linux__
#include <aio.h>
#include <signal.h>
#endif

namespace callback {

class AsyncFileSave : public std::enable_shared_from_this<AsyncFileSave> {

public:
  AsyncFileSave(const boost::url &url, const std::string &user = {},
                const std::string &password = {});
  AsyncFileSave(const AsyncFileSave &) = delete;
  AsyncFileSave(AsyncFileSave &&) = delete;
  AsyncFileSave &operator=(const AsyncFileSave &) = delete;
  AsyncFileSave &operator=(AsyncFileSave &&) = delete;

  virtual ~AsyncFileSave() noexcept = default;

  void Register(TaskScheduler *pSched);
  void SaveFileAtEndpoint(const std::filesystem::path &dst);

  size_t GetPendingRequestOperations() const { return socketCtxs_.size(); }
  size_t GetPendingFileOperations() const { return easyCtxs_.size(); }

private:
  boost::url url_;

  TaskScheduler *pSched_{nullptr};
  util::CurlMultiWrapper wCurlMulti_;
  TaskToken token_{};

#if _WIN32
  struct Win32Overlapped {
    HANDLE hFile{nullptr};
    OVERLAPPED overlapped{};
    LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine{nullptr};
  };
  using _CurlEasyContext =
      CurlEasyContext<Win32Overlapped, void *, AsyncFileSave>;
#elif __linux__
  struct LinuxAioFile {
    aiocb _aiocb{};
    std::vector<char> buf;
  };
  using _CurlEasyContext = CurlEasyContext<LinuxAioFile, void *, AsyncFileSave>;
#endif
  using _CurlSocketContext = CurlSocketContext<AsyncFileSave>;

  std::unordered_map<size_t, std::shared_ptr<_CurlEasyContext>> easyCtxs_;
  std::unordered_map<curl_socket_t, std::shared_ptr<_CurlSocketContext>>
      socketCtxs_;

  bool allowUpdate_{true};

  void CheckMultiInfo();

  static int SocketCallback(CURL *easy, curl_socket_t s, int action,
                            AsyncFileSave *AsyncFileSave,
                            _CurlSocketContext *curlSocketContext);
  static int TimeoutCallback(CURLM *multi, int timeoutMs,
                             AsyncFileSave *AsyncFileSave);

  static void BackgroundHandlerProc(void *curlSocketContext_clientData,
                                    int mask);
  static void TimeoutHandlerProc(void *asyncFileSave_clientData);
  static void DebounceUpdateProc(void *asyncFileSave_clientData);

  static void WriteFunction(char *contents, size_t sz, size_t nmemb,
                            void *pUserData);

#ifdef __linux__
  static void AioSigHandler(int sig, siginfo_t *si, void *ucontext);
  static void InstallHandlers();
#endif //  __linux__

#if _WIN32
  static VOID CALLBACK FileIOCompletionRoutine(
      __in DWORD dwErrorCode, __in DWORD dwNumberOfBytesTransferred,
      __in LPOVERLAPPED lpOverlapped);
#endif
};

} // namespace callback
