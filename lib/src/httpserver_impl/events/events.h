#ifndef EVENTS_H
#define EVENTS_H

#include <httpserver_impl/http/http_object.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace HTTPServer {

struct Event {
  virtual ~Event() = default;
};

struct ConnectionOpenedEvent : public Event {};

struct ConnectionClosedEvent : public Event {};

struct RequestProcessedEvent : public Event {
  const HttpResponse& response;
  std::optional<std::chrono::milliseconds> processing_time_ms;
  std::optional<uint64_t> bytes_received;
  std::optional<uint64_t> bytes_sent;

  RequestProcessedEvent(const HttpResponse& res,
                        std::optional<std::chrono::steady_clock::time_point>
                            start_time = std::nullopt,
                        std::optional<uint64_t> b_received = std::nullopt,
                        std::optional<uint64_t> b_sent = std::nullopt)
      : response(res), bytes_received(b_received), bytes_sent(b_sent) {
    if (start_time) {
      processing_time_ms =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - *start_time);
    }
  }
};

class IObserver {
 public:
  virtual ~IObserver() = default;
  virtual void onEvent(const Event& event) = 0;
};

}  // namespace HTTPServer

#endif
