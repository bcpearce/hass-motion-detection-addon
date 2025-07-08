#include "Util/CurlMultiWrapper.h"

#include <mutex>

namespace {
static std::once_flag curlGlobalFlag;
}

namespace util {

CurlMultiWrapper::CurlMultiWrapper() noexcept {
  std::call_once(curlGlobalFlag, curl_global_init, CURL_GLOBAL_ALL);
  pCurl_ = curl_multi_init();
}

CurlMultiWrapper::CurlMultiWrapper(CurlMultiWrapper &&other) noexcept
    : pCurl_(std::exchange(other.pCurl_, nullptr)) {}

CurlMultiWrapper &
CurlMultiWrapper::operator=(CurlMultiWrapper &&other) noexcept {
  if (this != &other) {
    this->pCurl_ = other.pCurl_;
    other.pCurl_ = nullptr;
  }
  return *this;
}

CurlMultiWrapper::~CurlMultiWrapper() noexcept {
  if (pCurl_) {
    curl_multi_cleanup(pCurl_);
  }
}

} // namespace util