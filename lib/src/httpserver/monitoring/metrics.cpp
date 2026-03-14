#include <httpserver/events/event_dispatcher.h>
#include <httpserver/monitoring/metrics.h>

namespace HTTPServer {

Metrics& Metrics::instance() {
  static Metrics instance;
  return instance;
}

Metrics::Metrics()
    : d_activeConnections(0),
      d_totalRequests(0),
      d_responses2xx(0),
      d_responses3xx(0),
      d_responses4xx(0),
      d_responses5xx(0),
      d_totalBytesReceived(0),
      d_totalBytesSent(0),
      d_totalRequestProcessingTimeMs(0) {
  EventDispatcher::instance().subscribe(this);
}

void Metrics::onEvent(const Event& event) {
  if (dynamic_cast<const ConnectionOpenedEvent*>(&event)) {
    incrementActiveConnection();
  } else if (dynamic_cast<const ConnectionClosedEvent*>(&event)) {
    decrementActiveConnection();
  } else if (auto e = dynamic_cast<const RequestProcessedEvent*>(&event)) {
    incrementTotalRequests();

    if (e->processing_time_ms) {
      recordRequestProcessingTime(*e->processing_time_ms);
    }

    if (e->bytes_received) {
      recordBytesReceived(*e->bytes_received);
    }

    if (e->bytes_sent) {
      recordBytesSent(*e->bytes_sent);
    }

    recordResponseStatus(e->response.code);
  }
}

void Metrics::incrementActiveConnection() {
  d_activeConnections.fetch_add(1, std::memory_order_relaxed);
}

void Metrics::decrementActiveConnection() {
  d_activeConnections.fetch_sub(1, std::memory_order_relaxed);
}

void Metrics::incrementTotalRequests() {
  d_totalRequests.fetch_add(1, std::memory_order_relaxed);
}

void Metrics::recordResponseStatus(StatusCode code) {
  int int_code = static_cast<int>(code);

  if (int_code < 100 || int_code > 599) {
    return;
  }

  switch (int_code / 100) {
    case 2:
      d_responses2xx.fetch_add(1, std::memory_order_relaxed);
      break;
    case 3:
      d_responses3xx.fetch_add(1, std::memory_order_relaxed);
      break;
    case 4:
      d_responses4xx.fetch_add(1, std::memory_order_relaxed);
      break;
    case 5:
      d_responses5xx.fetch_add(1, std::memory_order_relaxed);
      break;
    default:
      break;
  }
}

void Metrics::recordBytesReceived(uint64_t bytes) {
  d_totalBytesReceived.fetch_add(bytes, std::memory_order_relaxed);
}

void Metrics::recordBytesSent(uint64_t bytes) {
  d_totalBytesSent.fetch_add(bytes, std::memory_order_relaxed);
}

void Metrics::recordRequestProcessingTime(std::chrono::milliseconds duration) {
  d_totalRequestProcessingTimeMs.fetch_add(
      static_cast<uint64_t>(duration.count()), std::memory_order_relaxed);
}

MetricsSnapshot Metrics::snapshot() const {
  return MetricsSnapshot(
      d_activeConnections.load(std::memory_order_relaxed),
      d_totalRequests.load(std::memory_order_relaxed),
      d_responses2xx.load(std::memory_order_relaxed),
      d_responses3xx.load(std::memory_order_relaxed),
      d_responses4xx.load(std::memory_order_relaxed),
      d_responses5xx.load(std::memory_order_relaxed),
      d_totalBytesReceived.load(std::memory_order_relaxed),
      d_totalBytesSent.load(std::memory_order_relaxed),
      d_totalRequestProcessingTimeMs.load(std::memory_order_relaxed));
}

}  // namespace HTTPServer