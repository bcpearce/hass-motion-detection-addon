#include "Util/BufferOperations.h"

#include <cstring>

#include <algorithm>
#include <ranges>
#include <span>
#include <string>
#include <vector>

namespace util {

size_t FillBufferCallback(char *contents, size_t sz, size_t nmemb,
                          void *pUserData) {
  if (pUserData) {
    std::vector<char> &buf = *static_cast<std::vector<char> *>(pUserData);
    size_t realsize = sz * nmemb;
    buf.insert_range(buf.end(), std::span<char>(contents, realsize));
    return realsize;
  }
  return 0;
}

size_t SendBufferCallback(char *dest, size_t sz, size_t nmemb,
                          void *pUserData) {

  const size_t bufferSz = sz * nmemb;

  if (pUserData) {
    std::string *buf = static_cast<std::string *>(pUserData);
    size_t copyThisMuch = std::min(bufferSz, buf->size());
    memcpy(dest, buf->data(), copyThisMuch);

    *buf = buf->substr(copyThisMuch);
    return copyThisMuch;
  }
  return 0;
}

} // namespace util