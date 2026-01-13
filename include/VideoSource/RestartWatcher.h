#pragma once

#include "Logger.h"
#include <chrono>
#include <functional>
#include <memory>
#include <vector>

namespace video_source {

template <class Callback_t = std::function<void(int)>> class RestartWatcher {
public:
  static void CheckAndRestart(void *clientData_restartWatcher) {
    if (clientData_restartWatcher) {
      auto &restartWatcher =
          *static_cast<RestartWatcher *>(clientData_restartWatcher);
      auto spSource = restartWatcher.wpSource.lock();
      auto spSched = restartWatcher.wpSched.lock();
      if (spSource && !spSource->IsActive()) {
        // attempt to restore
        LOGGER->info("Video Source {} is down, attempting to restart...",
                     restartWatcher.sourceDesc);
        for (auto &wp : restartWatcher.wpCallbacks) {
          if (auto sp = wp.lock()) {
            // send a null payload to the callbacks
            (*sp)({});
            ++restartWatcher.nullPayloadUpdates_;
          }
        }
        spSource->StartStream();
        ++restartWatcher.restartAttempts_;
        restartWatcher.interval =
            std::min(restartWatcher.interval * 2, restartWatcher.maxInterval);
        LOGGER->info("Checking Video Source {} for status in {} seconds",
                     restartWatcher.sourceDesc,
                     std::chrono::duration_cast<std::chrono::seconds>(
                         restartWatcher.interval)
                         .count());
      } else {
        restartWatcher.interval = restartWatcher.minInterval;
      }
      if (spSched) {
        spSched->scheduleDelayedTask(restartWatcher.interval.count(),
                                     CheckAndRestart,
                                     clientData_restartWatcher);
      }
    }
  }

  RestartWatcher(std::string sourceDesc,
                 std::shared_ptr<video_source::VideoSource> pSource,
                 std::shared_ptr<TaskScheduler> pSched)
      : sourceDesc{std::move(sourceDesc)}, wpSource{pSource}, wpSched{pSched} {
    if (pSource && pSched) {
      pSched->scheduleDelayedTask(interval.count(), CheckAndRestart, this);
    }
  }

  int GetRestartAttempts() const { return restartAttempts_; }
  int GetNullPayloadUpdates() const { return nullPayloadUpdates_; }

  std::string sourceDesc;
  std::weak_ptr<video_source::VideoSource> wpSource;
  std::weak_ptr<TaskScheduler> wpSched;
  std::vector<std::weak_ptr<Callback_t>> wpCallbacks;

  std::chrono::microseconds minInterval{std::chrono::seconds(3)};
  std::chrono::microseconds maxInterval{std::chrono::seconds(60)};
  std::chrono::microseconds interval{minInterval};

private:
  int restartAttempts_{0};
  int nullPayloadUpdates_{0};
};

} // namespace video_source