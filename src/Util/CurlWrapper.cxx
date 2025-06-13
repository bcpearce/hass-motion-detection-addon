#include "Util/CurlWrapper.h"

#include <mutex>

namespace {
static std::once_flag curlGlobalFlag;
}

namespace util {

CurlWrapper::CurlWrapper() noexcept {
  std::call_once(curlGlobalFlag, curl_global_init, CURL_GLOBAL_ALL);
  errBuf_.fill('\0');
  curl = curl_easy_init();
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errBuf_.data());
#if defined(_DEBUG)
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
#endif
}

CurlWrapper::~CurlWrapper() noexcept {
  if (curl) {
    curl_easy_cleanup(curl);
  }
}

} // namespace util