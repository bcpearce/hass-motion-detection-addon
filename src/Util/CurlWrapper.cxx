#include "Util/CurlWrapper.h"

#include <mutex>

namespace {
static std::once_flag curlGlobalFlag;
}

namespace util {

CurlWrapper::CurlWrapper() noexcept {
  std::call_once(curlGlobalFlag, curl_global_init, CURL_GLOBAL_ALL);
  errBuf_.fill('\0');
  pCurl_ = curl_easy_init();
  curl_easy_setopt(pCurl_, CURLOPT_ERRORBUFFER, errBuf_.data());
#if defined(_DEBUG)
  curl_easy_setopt(pCurl_, CURLOPT_VERBOSE, 1);
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