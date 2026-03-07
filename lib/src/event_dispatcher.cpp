#include "httpserver/event_dispatcher.h"

namespace HTTPServer {

EventDispatcher& EventDispatcher::instance() {
  static EventDispatcher dispatcher;
  return dispatcher;
}

void EventDispatcher::subscribe(IObserver* observer) {
  std::lock_guard<std::mutex> lock(d_mtx);
  d_observers.push_back(observer);
}

void EventDispatcher::unsubscribe(IObserver* observer) {
  std::lock_guard<std::mutex> lock(d_mtx);
  d_observers.erase(
      std::remove(d_observers.begin(), d_observers.end(), observer),
      d_observers.end());
}

void EventDispatcher::dispatch(const Event& event) {
  std::lock_guard<std::mutex> lock(d_mtx);
  for (auto* observer : d_observers) {
    observer->onEvent(event);
  }
}

}  // namespace HTTPServer
