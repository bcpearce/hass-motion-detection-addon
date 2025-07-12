#include "Logger.h"

#include "HomeAssistant/SyncHassHandler.h"

#include <chrono>
#include <format>
#include <iostream>
#include <string_view>

#include "HomeAssistant/Json.h"
#include "Util/BufferOperations.h"
#include "Util/CurlWrapper.h"

using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;

namespace home_assistant {

SyncHassHandler::SyncHassHandler(const boost::url &url,
                                 const std::string &token,
                                 const std::string &entityId)
    : BaseHassHandler(url, token, entityId) {}

void SyncHassHandler::UpdateState_Impl(std::string_view state,
                                       const json &attributes) {
  thread_local std::string payload;
  thread_local util::CurlWrapper wCurl;

  PrepareGetRequest(wCurl, buf_);
  wCurl(curl_easy_perform);
  HandleGetResponse(wCurl, buf_);

  UpdateStateInternal(state, attributes);

  PreparePostRequest(wCurl, buf_, payload);
  wCurl(curl_easy_perform);
  HandlePostResponse(wCurl, buf_);
}

} // namespace home_assistant