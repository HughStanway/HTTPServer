#include "httpserver/periodic_idle_ip_cleanup.h"

#include "httpserver/logger.h"

namespace HTTPServer {

PerioidIdleIpCleanup::PerioidIdleIpCleanup(IpMap& connected_ips,
                                           std::mutex& mtx)
    : d_connected_ips(connected_ips), d_mtx(mtx) {
  start();
}

PerioidIdleIpCleanup::~PerioidIdleIpCleanup() { stop(); }

void PerioidIdleIpCleanup::start() {
  if (d_running.exchange(true)) {
    return;  // Already running
  }
  d_thread = std::thread(&PerioidIdleIpCleanup::run, this);
}

void PerioidIdleIpCleanup::stop() {
  if (d_running.exchange(false)) {
    return;
  }

  d_cv.notify_all();

  if (d_thread.joinable()) {
    d_thread.join();
  }
  LOG_INFO("Shutdown: Periodic idle IP cleanup thread stopped.");
}

void PerioidIdleIpCleanup::run() {
  std::string name = "cleanup-thread";
#if defined(__APPLE__)
  pthread_setname_np(name.c_str());
#elif defined(__linux__)
  pthread_setname_np(pthread_self(), name.c_str());
#endif
  LOG_INFO("Periodic idle IP cleanup thread started");

  while (d_running) {
    LOG_INFO("Periodic idle IP cleanup sleeping for " +
             std::to_string(kCleanupInterval.count()) + " minutes");
    std::unique_lock lock(d_cv_mtx);
    d_cv.wait_for(lock, kCleanupInterval, [this] { return !d_running.load(); });

    if (d_running) {
      cleanup_idle_ips();
    }
  }
}

void PerioidIdleIpCleanup::cleanup_idle_ips() {
  const auto now = std::chrono::steady_clock::now();

  std::lock_guard lock(d_mtx);
  for (auto it = d_connected_ips.begin(); it != d_connected_ips.end();) {
    if (it->second.active == 0 && now - it->second.last_seen > kIdleTimeout) {
      LOG_INFO("Idle IP address " + it->first + " erased");
      it = d_connected_ips.erase(it);
    } else {
      ++it;
    }
  }
  LOG_INFO("Periodic idle IP cleanup finished processing.");
}

}  // namespace HTTPServer