#pragma once

#include <cstddef>

namespace util {

size_t FillBufferCallback(char *contents, size_t sz, size_t nmemb,
                          void *pUserData);

size_t SendBufferCallback(char *dest, size_t sz, size_t nmemb, void *pUserData);

} // namespace util