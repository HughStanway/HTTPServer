#ifndef EVENT_DISPATCHER_H
#define EVENT_DISPATCHER_H

#include <httpserver/events/events.h>

#include <algorithm>
#include <mutex>
#include <vector>

namespace HTTPServer {

class EventDispatcher {
 public:
  static EventDispatcher& instance();

  void subscribe(IObserver* observer);
  void unsubscribe(IObserver* observer);
  void dispatch(const Event& event);

 private:
  EventDispatcher() = default;
  ~EventDispatcher() = default;
  EventDispatcher(const EventDispatcher&) = delete;
  EventDispatcher& operator=(const EventDispatcher&) = delete;

  std::vector<IObserver*> d_observers;
  std::mutex d_mtx;
};

}  // namespace HTTPServer

#endif
