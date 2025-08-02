#include "Logger.h"

#include "Callback/AsyncDebouncer.h"

namespace callback {

AsyncDebouncer::AsyncDebouncer(std::shared_ptr<TaskScheduler> pSched)
    : pSched_{pSched} {}

void AsyncDebouncer::Debounce(std::chrono::microseconds delay) {
  if (delay.count() <= 0) {
    if (!updateAllowed_) {
      pSched_->unscheduleDelayedTask(taskToken_);
    }
    updateAllowed_ = true;
  }
  if (!updateAllowed_) { // reschedule
    pSched_->rescheduleDelayedTask(taskToken_, delay.count(),
                                   DebounceUpdateProc, this);
  } else { //
    taskToken_ =
        pSched_->scheduleDelayedTask(delay.count(), DebounceUpdateProc, this);
    updateAllowed_ = false;
  }
}

void AsyncDebouncer::DebounceUpdateProc(void *asyncDebouncer_clientData) {
  if (asyncDebouncer_clientData) {
    AsyncDebouncer *pDebouncer =
        static_cast<AsyncDebouncer *>(asyncDebouncer_clientData);
    LOGGER->debug("Restoring update ability to {}", typeid(*pDebouncer).name());
    pDebouncer->updateAllowed_ = true;
  }
}
} // namespace callback