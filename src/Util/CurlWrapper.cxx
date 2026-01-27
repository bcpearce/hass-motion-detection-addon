#if defined(_DEBUG)
#include "Logger.h"
#include <ranges>
#endif

#include "Util/CurlWrapper.h"

#include <mutex>

namespace {
static std::once_flag curlGlobalFlag;

#if defined(_DEBUG)
int curlDebugCallback(CURL *, curl_infotype type, char *data, size_t sz,
                      void *) {
  switch (type) {
  case CURLINFO_TEXT: {
    const std::string_view log(data, sz);
    for (const auto &lv : log | std::views::split('\n')) {
      std::string_view line(lv.begin(), lv.end());
      line = line.substr(0, line.find_last_not_of("\r\n"));
      if (!line.empty()) {
        LOGGER->debug("{} ", line);
      }
    }
  } break;
  default:
    break;
  }
  return 0;
}
#endif

} // namespace

namespace util {

CurlWrapper::CurlWrapper() noexcept {
  std::call_once(curlGlobalFlag, curl_global_init, CURL_GLOBAL_ALL);
  errBuf_.fill('\0');
  pCurl_ = curl_easy_init();
  curl_easy_setopt(pCurl_, CURLOPT_ERRORBUFFER, errBuf_.data());
#if defined(_DEBUG)
  curl_easy_setopt(pCurl_, CURLOPT_VERBOSE, 1);
  curl_easy_setopt(pCurl_, CURLOPT_DEBUGFUNCTION, curlDebugCallback);
#endif
}

CurlWrapper::CurlWrapper(CurlWrapper &&other) noexcept
    : pCurl_(std::exchange(other.pCurl_, nullptr)),
      errBuf_(std::exchange(other.errBuf_, {})) {}

CurlWrapper &CurlWrapper::operator=(CurlWrapper &&other) noexcept {
  if (this != &other) {
    this->pCurl_ = other.pCurl_;
    other.pCurl_ = nullptr;
    this->errBuf_ = std::move(other.errBuf_);
  }
  return *this;
}

CurlWrapper::~CurlWrapper() noexcept {
  if (pCurl_) {
    curl_easy_cleanup(pCurl_);
  }
}

} // namespace util