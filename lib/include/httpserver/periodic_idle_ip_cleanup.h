#ifndef PERIODIC_IDLE_IP_CLEANUP_H
#define PERIODIC_IDLE_IP_CLEANUP_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include "connection_guard.h"

namespace HTTPServer {

class PerioidIdleIpCleanup {
 public:
  using IpMap = std::unordered_map<std::string, ConnectedIp>;

  PerioidIdleIpCleanup(IpMap& connected_ips, std::mutex& mtx);
  ~PerioidIdleIpCleanup();
  PerioidIdleIpCleanup(const PerioidIdleIpCleanup&) = delete;
  PerioidIdleIpCleanup& operator=(const PerioidIdleIpCleanup&) = delete;

  void start();
  void stop();

 private:
  static constexpr auto kCleanupInterval = std::chrono::minutes(10);
  static constexpr auto kIdleTimeout = std::chrono::minutes(5);

  IpMap& d_connected_ips;
  std::mutex& d_mtx;
  std::atomic<bool> d_running{false};
  std::thread d_thread;
  std::condition_variable d_cv;
  std::mutex d_cv_mtx;

  void run();
  void cleanup_idle_ips();
};

}  // namespace HTTPServer

#endif