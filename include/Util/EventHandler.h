#ifndef INCLUDE_EVENT_HANDLER_H
#define INCLUDE_EVENT_HANDLER_H

#include <functional>
#include <ranges>
#include <shared_mutex>
#include <unordered_map>

namespace util {

template <typename Payload> class EventHandler {
public:
  virtual ~EventHandler() = default;

  int Subscribe(std::function<void(Payload)> callback) {
    static int nextId{0};
    std::scoped_lock lk(mtx_);
    callbacks_[nextId] = callback;
    return nextId++;
  }
  void Unsubscribe(int callbackId) {}

protected:
  virtual void OnEvent(Payload data) {
    std::scoped_lock lk(mtx_);
    for (auto &callback : callbacks_ | std::views::values) {
      callback(data);
    }
  }

private:
  std::unordered_map<int, std::function<void(Payload)>> callbacks_;
  std::shared_mutex mtx_;
};

} // namespace util

#endif // !INCLUDE_EVENT_HANDLER_H
