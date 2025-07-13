#ifndef INCLUDE_CALLBACK_CONTEXT_H
#define INCLUDE_CALLBACK_CONTEXT_H

#include <memory>
#include <string>
#include <string_view>

#include "Util/CurlWrapper.h"

namespace callback {

template <class WriteData_t, class ReadData_t = void *, class Handler = void *>
struct CurlEasyContext {
  util::CurlWrapper wCurl;
  WriteData_t writeData;
  ReadData_t readData;
  size_t contextId{0};
  std::weak_ptr<Handler> pHandler;
};

template <class Handler>
struct CurlSocketContext
    : std::enable_shared_from_this<CurlSocketContext<Handler>> {
  curl_socket_t sockfd{};
  std::weak_ptr<Handler> pHandler;
};

} // namespace callback

#endif