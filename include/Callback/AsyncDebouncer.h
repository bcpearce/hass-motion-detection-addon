#pragma once

#include <chrono>
#include <gsl/gsl>

#include <UsageEnvironment.hh>

namespace callback {

class AsyncDebouncer {
public:
  explicit AsyncDebouncer(TaskScheduler *pSched);
  AsyncDebouncer(const AsyncDebouncer &) = delete;
  AsyncDebouncer(AsyncDebouncer &&) = delete;
  AsyncDebouncer &operator=(const AsyncDebouncer &) = delete;
  AsyncDebouncer &operator=(AsyncDebouncer &&) = delete;

  virtual ~AsyncDebouncer() noexcept = default;

  template <typename Dur> inline void Debounce(Dur delay) {
    Debounce(std::chrono::duration_cast<std::chrono::microseconds>(delay));
  }

  void Debounce(std::chrono::microseconds delay); // or reschedule

  [[nodiscard]] bool UpdateAllowed() const { return updateAllowed_; }

private:
  static void DebounceUpdateProc(void *asyncDebouncer_clientData);

  gsl::not_null<TaskScheduler *> pSched_;
  TaskToken taskToken_{nullptr};
  bool updateAllowed_{true};
};

} // namespace callback