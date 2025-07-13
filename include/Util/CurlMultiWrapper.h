#pragma once

#include <array>
#include <exception>
#include <format>
#include <string_view>
#include <utility>

#include <curl/curl.h>

namespace util {

class CurlMultiWrapper {
public:
  class CurlMultiError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
  };

  CurlMultiWrapper() noexcept;
  CurlMultiWrapper(const CurlMultiWrapper &) = delete;
  CurlMultiWrapper &operator=(const CurlMultiWrapper &) = delete;
  CurlMultiWrapper(CurlMultiWrapper &&) noexcept;
  CurlMultiWrapper &operator=(CurlMultiWrapper &&) noexcept;

  ~CurlMultiWrapper() noexcept;
  CURLM *pCurl_{nullptr};
  CURLM *operator&() { return pCurl_; }
  operator bool() const { return bool(pCurl_); }

  template <typename F, typename... Ts> CURLMcode call(F func, Ts... args) {
    const CURLMcode res = func(pCurl_, args...);
    if (res != CURLMcode::CURLM_OK) {
      throw CurlMultiError(
          std::format("Error {} calling CURL MULTI function {}: {}", int(res),
                      typeid(func).name(), curl_multi_strerror(res)));
    }
    return res;
  }

  template <typename F, typename... Ts>
  inline CURLMcode operator()(F func, Ts... args) {
    return this->call(func, args...);
  }
};

} // namespace util