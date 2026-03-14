#ifndef CONNECTION_GUARD_H
#define CONNECTION_GUARD_H

#include <httpserver/events/event_dispatcher.h>
#include <httpserver/events/events.h>
#include <httpserver/monitoring/log_event.h>
#include <httpserver/monitoring/logger.h>
#include <httpserver/utils/config.h>

#include <chrono>
#include <mutex>

namespace HTTPServer {

struct ConnectedIp {  // This should be moved and tokens should have a separate
                      // struct - understand where this struct is used first
  int active = 0;
  double tokens = Config::get().kMaxTokens;
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
    }
    EventDispatcher::instance().dispatch(ConnectionOpenedEvent{});
    LOG_EVENT(LogLevel::INFO,
              LogEvent("connection_guard_enter")
                  .add("client_fd", d_client_fd)
                  .add("active_connections", d_connection.active));
  }

  ~ConnectionGuard() {
    {
      std::lock_guard lock(d_mtx);
      d_connection.active--;
    }
    EventDispatcher::instance().dispatch(ConnectionClosedEvent{});
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
