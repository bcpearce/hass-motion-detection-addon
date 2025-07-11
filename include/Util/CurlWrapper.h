#ifndef INCLUDE_UTIL_CURL_WRAPPER_H
#define INCLUDE_UTIL_CURL_WRAPPER_H

#include <array>
#include <exception>
#include <format>
#include <string_view>
#include <utility>

#include <curl/curl.h>

namespace util {

class CurlWrapper {
public:
  class CurlError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
  };

  CurlWrapper() noexcept;
  CurlWrapper(const CurlWrapper &) = delete;
  CurlWrapper &operator=(const CurlWrapper &) = delete;
  CurlWrapper(CurlWrapper &&) noexcept;
  CurlWrapper &operator=(CurlWrapper &&) noexcept;

  ~CurlWrapper() noexcept;
  CURL *pCurl_{nullptr};
  CURL *operator&() { return pCurl_; }
  operator bool() const { return bool(pCurl_); }

  template <typename F, typename... Ts> CURLcode call(F func, Ts... args) {
    const CURLcode res = func(pCurl_, args...);
    if (res != CURLcode::CURLE_OK) {
      throw CurlError(
          std::format("Error {} calling CURL function {}: {}", int(res),
                      typeid(func).name(),
                      std::string_view(errBuf_.data(), errBuf_.size())));
    }
    return res;
  }

  template <typename F, typename... Ts>
  inline CURLcode operator()(F func, Ts... args) {
    return this->call(func, args...);
  }

private:
  std::array<char, CURL_ERROR_SIZE> errBuf_;
};

} // namespace util

#endif