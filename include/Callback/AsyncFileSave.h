#pragma once

#include "WindowsWrapper.h"

#include "Callback/Context.h"

#include <deque>
#include <filesystem>
#include <memory>
#include <string_view>
#include <unordered_map>

#include <UsageEnvironment.hh>
#include <boost/circular_buffer.hpp>
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
  AsyncFileSave(const std::filesystem::path &dstPath,
                const boost::url &url = {}, const std::string &user = {},
                const std::string &password = {});
  AsyncFileSave(const AsyncFileSave &) = delete;
  AsyncFileSave(AsyncFileSave &&) = delete;
  AsyncFileSave &operator=(const AsyncFileSave &) = delete;
  AsyncFileSave &operator=(AsyncFileSave &&) = delete;

  virtual ~AsyncFileSave() noexcept;

  void Register(TaskScheduler *pSched);
  void SaveFileAtEndpoint(const std::filesystem::path &dst = {});

  size_t GetPendingRequestOperations() const { return socketCtxs_.size(); }
  size_t GetPendingFileOperations() const { return easyCtxs_.size(); }

  const boost::circular_buffer<std::filesystem::path> &
  GetSavedFilePaths() const;

  void SetLimitSavedFilePaths(size_t limit);

  size_t defaultJpgBufferSize{2 * 1024 * 1024}; // default to 2Mb
  size_t defaultSavedFilePathsSize{200};

private:
  boost::url url_;
  std::string user_;
  std::string password_;
  std::filesystem::path dstPath_;

  boost::circular_buffer<std::filesystem::path> savedFilePaths_;

  TaskScheduler *pSched_{nullptr};
  util::CurlMultiWrapper wCurlMulti_;
  TaskToken token_{};

#if _WIN32
  struct Win32Overlapped {
    HANDLE hFile{INVALID_HANDLE_VALUE};
    OVERLAPPED overlapped{.hEvent{0}};
    LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine{nullptr};
    std::vector<char> buf;
    std::filesystem::path dstPath;

    ~Win32Overlapped() noexcept;
  };
  using _CurlEasyContext =
      CurlEasyContext<Win32Overlapped, void *, AsyncFileSave>;
#elif __linux__
  struct LinuxAioFile {
    aiocb _aiocb{};
    std::vector<char> buf;
    std::filesystem::path dstPath;

    ~LinuxAioFile() noexcept;
  };
  using _CurlEasyContext = CurlEasyContext<LinuxAioFile, void *, AsyncFileSave>;
#endif
  using _CurlSocketContext = CurlSocketContext<AsyncFileSave>;

  std::unordered_map<size_t, std::shared_ptr<_CurlEasyContext>> easyCtxs_;
  std::vector<char> spareBuf_;
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

#ifdef __linux__
  static void AioSigHandler(int sig, siginfo_t *si, void *ucontext);
  static void InstallHandlers();
#endif //  __linux__

#if _WIN32
  static VOID CALLBACK FileIOCompletionRoutine(
      __in DWORD dwErrorCode, __in DWORD dwNumberOfBytesTransferred,
      __in LPOVERLAPPED lpOverlapped);
#endif

  static void RemoveContext(_CurlEasyContext *pCtx);
};

} // namespace callback
