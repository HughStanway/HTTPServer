#include <httpserver/core/connection_manager.h>
#include <httpserver/monitoring/log_event.h>
#include <httpserver/monitoring/logger.h>

#include <algorithm>

namespace HTTPServer {

ConnectionManager& ConnectionManager::instance() {
  static ConnectionManager instance;
  return instance;
}

bool ConnectionManager::canAcceptConnection(const std::string& ip) {
  std::lock_guard lock(d_mtx);
  auto& entry = d_ips[ip];

  if (entry.active >= Config::get().kMaxConnectionsPerIp) {
    LOG_EVENT(LogLevel::WARN,
              LogEvent("max_number_connections_from_ip_exceeded")
                  .add("ip", ip)
                  .add("active_connections", entry.active));
    return false;
  }
  return true;
}

void ConnectionManager::addConnection(const std::string& ip) {
  std::lock_guard lock(d_mtx);
  d_ips[ip].active++;
  LOG_EVENT(LogLevel::INFO, LogEvent("connection_manager_add")
                                .add("ip", ip)
                                .add("active_connections", d_ips[ip].active));
}

void ConnectionManager::removeConnection(const std::string& ip) {
  std::lock_guard lock(d_mtx);
  if (auto it = d_ips.find(ip); it != d_ips.end()) {
    it->second.active--;
    LOG_EVENT(
        LogLevel::INFO,
        LogEvent("connection_manager_remove")
            .add("ip", ip)
            .add("active_connections", it->second.active));
  }
}

bool ConnectionManager::allowRequest(const std::string& ip) {
  std::lock_guard lock(d_mtx);
  auto& entry = d_ips[ip];
  
  auto now = std::chrono::steady_clock::now();
  std::chrono::duration<double> elapsed = now - entry.tokenBucket.last_seen;

  entry.tokenBucket.tokens =
      std::min(Config::get().kMaxTokens,
               entry.tokenBucket.tokens + elapsed.count() * Config::get().kRefillRate);
  entry.tokenBucket.last_seen = now;

  if (entry.tokenBucket.tokens < 1.0) {
    LOG_EVENT(LogLevel::WARN,
              LogEvent("rate_limit_exceeded").add("ip", ip));
    return false;
  }
  entry.tokenBucket.tokens -= 1.0;
  return true;
}

void ConnectionManager::removeIdleConnections() {
  const auto now = std::chrono::steady_clock::now();
  const auto timeout = Config::get().kIdleTimeout;

  std::lock_guard lock(d_mtx);
  size_t removed = 0;
  for (auto it = d_ips.begin(); it != d_ips.end();) {
    if (it->second.active == 0 &&
        now - it->second.tokenBucket.last_seen > timeout) {
      std::string ip = it->first;
      it = d_ips.erase(it);
      removed++;
      LOG_EVENT(LogLevel::INFO, LogEvent("idle_ip_entry_erased").add("ip", ip));
    } else {
      ++it;
    }
  }
  
  LOG_EVENT(LogLevel::INFO,
            LogEvent("idle_ip_cleanup_finished").add("removed", removed));
}

}  // namespace HTTPServer
