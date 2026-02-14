#include "httpserver/periodic_idle_ip_cleanup.h"

#include "httpserver/logger.h"
#include "httpserver/log_event.h"

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
  LOG_EVENT(LogLevel::INFO, LogEvent("idle_ip_cleanup_thread_stopped"));
}

void PerioidIdleIpCleanup::run() {
  std::string name = "cleanup-thread";
#if defined(__APPLE__)
  pthread_setname_np(name.c_str());
#elif defined(__linux__)
  pthread_setname_np(pthread_self(), name.c_str());
#endif
  LOG_EVENT(LogLevel::INFO, LogEvent("idle_ip_cleanup_thread_started"));

  while (d_running) {
    LOG_EVENT(LogLevel::INFO,
              LogEvent("idle_ip_cleanup_sleeping")
                  .add("minutes",
                       static_cast<int>(Config::get().kCleanupInterval.count())));
    std::unique_lock lock(d_cv_mtx);
    d_cv.wait_for(lock, Config::get().kCleanupInterval,
                  [this] { return !d_running.load(); });

    if (d_running) {
      cleanup_idle_ips();
    }
  }
}

void PerioidIdleIpCleanup::cleanup_idle_ips() {
  const auto now = std::chrono::steady_clock::now();

  std::lock_guard lock(d_mtx);
  size_t removed = 0;
  for (auto it = d_connected_ips.begin(); it != d_connected_ips.end();) {
    if (it->second.active == 0 && now - it->second.last_seen > Config::get().kIdleTimeout) {
      std::string ip = it->first;
      it = d_connected_ips.erase(it);
      removed++;
      LOG_EVENT(LogLevel::INFO,
                LogEvent("idle_ip_entry_erased").add("ip", ip));
    } else {
      ++it;
    }
  }
  LOG_EVENT(LogLevel::INFO,
            LogEvent("idle_ip_cleanup_finished").add("removed", removed));
}

}  // namespace HTTPServer
