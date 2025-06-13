#ifndef UTIL_CURL_WRAPPER_H
#define UTIL_CURL_WRAPPER_H

#include <array>
#include <exception>
#include <format>
#include <string_view>

#include <curl/curl.h>

namespace util {

class CurlWrapper {
public:
  class CurlError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
  };

  CurlWrapper() noexcept;
  ~CurlWrapper() noexcept;
  CURL *curl{nullptr};
  CURL *operator&() { return curl; }
  operator bool() const { return bool(curl); }

  template <typename F, typename... Ts> CURLcode call(F func, Ts... args) {
    const CURLcode res = func(curl, args...);
    if (res != CURLE_OK) {
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