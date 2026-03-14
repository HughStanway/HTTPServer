#ifndef METRICS_H
#define METRICS_H

#include <httpserver/events/events.h>
#include <httpserver/http/http_object.h>

#include <atomic>
#include <chrono>
#include <cstdint>

namespace HTTPServer {

struct MetricsSnapshot {
  uint64_t d_activeConnections;
  uint64_t d_totalRequests;
  uint64_t d_responses2xx;
  uint64_t d_responses3xx;
  uint64_t d_responses4xx;
  uint64_t d_responses5xx;
  uint64_t d_totalBytesReceived;
  uint64_t d_totalBytesSent;
  uint64_t d_totalRequestProcessingTimeMs;
};

class Metrics : public IObserver {
 public:
  static Metrics& instance();

  void onEvent(const Event& event) override;

  void incrementActiveConnection();
  void decrementActiveConnection();
  void incrementTotalRequests();
  void recordResponseStatus(StatusCode code);
  void recordBytesReceived(uint64_t bytes);
  void recordBytesSent(uint64_t bytes);
  void recordRequestProcessingTime(std::chrono::milliseconds duration);
  MetricsSnapshot snapshot() const;

 private:
  Metrics();
  ~Metrics() = default;

  Metrics(const Metrics&) = delete;
  Metrics& operator=(const Metrics&) = delete;
  Metrics(Metrics&&) = delete;
  Metrics& operator=(Metrics&&) = delete;

  std::atomic<uint64_t> d_activeConnections;
  std::atomic<uint64_t> d_totalRequests;
  std::atomic<uint64_t> d_responses2xx;
  std::atomic<uint64_t> d_responses3xx;
  std::atomic<uint64_t> d_responses4xx;
  std::atomic<uint64_t> d_responses5xx;
  std::atomic<uint64_t> d_totalBytesReceived;
  std::atomic<uint64_t> d_totalBytesSent;
  std::atomic<uint64_t> d_totalRequestProcessingTimeMs;
};

}  // namespace HTTPServer

#endif