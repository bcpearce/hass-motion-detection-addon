#include "Logger.h"

#include "HomeAssistant/ThreadedHassHandler.h"

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

ThreadedHassHandler::ThreadedHassHandler(const boost::url &url,
                                         const std::string &token,
                                         const std::string &entityId)
    : BaseHassHandler(url, token, entityId) {}

void ThreadedHassHandler::Start() {
  util::CurlWrapper wCurl;

  PrepareGetRequest(wCurl, buf_);
  wCurl(curl_easy_perform);
  HandleGetResponse(wCurl, buf_);

  updaterThread_ = std::jthread(
      [this, wCurl = std::move(wCurl)](std::stop_token stopToken) mutable {
#ifdef _WIN32
        SetThreadDescription(GetCurrentThread(),
                             L"Home Assistant Handler Sensor Update Thread");
#endif
        using sc = std::chrono::steady_clock;

        // Delay updating the state initially as the feed is still stabilizing
        sc::time_point lastStateUpdate =
            sc::now() +
            std::chrono::duration_cast<std::chrono::seconds>(debounceTime);
        std::unique_lock lk(mtx_);
        do {
          cv_.wait_for(lk, 5s);

          const bool stateChanging = IsStateChanging();
          const bool debounce = (sc::now() - lastStateUpdate > debounceTime);

          if ((stateChanging && debounce) || IsStateBecomingUnknown()) {
            buf_.clear();
            try {
              thread_local std::string payload; // reusable buffer
              PreparePostRequest(wCurl, buf_, payload);
              wCurl(curl_easy_perform);
              HandlePostResponse(wCurl, buf_);
              lastStateUpdate = sc::now();
            } catch (const std::exception &e) {
              LOGGER->error(e.what());
            }
          }
        } while (!stopToken.stop_requested());
      });
}

void ThreadedHassHandler::Stop() { updaterThread_ = {}; }

void ThreadedHassHandler::UpdateState_Impl(std::string_view state,
                                           const json &attributes) {
  std::unique_lock lk(mtx_);
  UpdateStateInternal(state, attributes);
  cv_.notify_all();
}

} // namespace home_assistant