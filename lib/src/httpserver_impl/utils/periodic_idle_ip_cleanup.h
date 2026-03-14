#ifndef PERIODIC_IDLE_IP_CLEANUP_H
#define PERIODIC_IDLE_IP_CLEANUP_H

#include <httpserver_impl/core/connection_manager.h>
#include <httpserver_impl/utils/config.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace HTTPServer {

class PerioidIdleIpCleanup {
 public:
  PerioidIdleIpCleanup();
  ~PerioidIdleIpCleanup();
  PerioidIdleIpCleanup(const PerioidIdleIpCleanup&) = delete;
  PerioidIdleIpCleanup& operator=(const PerioidIdleIpCleanup&) = delete;

  void start();
  void stop();

 private:
  std::atomic<bool> d_running{false};
  std::thread d_thread;
  std::condition_variable d_cv;
  std::mutex d_cv_mtx;

  void run();
};

}  // namespace HTTPServer

#endif