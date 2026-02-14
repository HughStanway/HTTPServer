#ifndef CONNECTION_GUARD_H
#define CONNECTION_GUARD_H

#include <chrono>
#include <mutex>

#include "log_event.h"
#include "logger.h"

namespace HTTPServer {

struct ConnectedIp {
  int active = 0;
  double tokens = 10.0;
  std::chrono::steady_clock::time_point last_seen =
      std::chrono::steady_clock::now();
};

class ConnectionGuard {
 public:
  ConnectionGuard(int client_fd, std::mutex& mtx, ConnectedIp& connection)
      : d_client_fd(client_fd), d_mtx(mtx), d_connection(connection) {
    {
      std::lock_guard lock(d_mtx);
      d_connection.active++;
      d_connection.last_seen = std::chrono::steady_clock::now();
    }
    LOG_EVENT(LogLevel::INFO,
              LogEvent("connection_guard_enter")
                  .add("client_fd", d_client_fd)
                  .add("active_connections", d_connection.active));
  }

  ~ConnectionGuard() {
    {
      std::lock_guard lock(d_mtx);
      d_connection.active--;
      d_connection.last_seen = std::chrono::steady_clock::now();
    }
    LOG_EVENT(LogLevel::INFO,
              LogEvent("connection_guard_exit")
                  .add("client_fd", d_client_fd)
                  .add("active_connections", d_connection.active));
  }

 private:
  int d_client_fd;
  std::mutex& d_mtx;
  ConnectedIp& d_connection;
};

}  // namespace HTTPServer

#endif
