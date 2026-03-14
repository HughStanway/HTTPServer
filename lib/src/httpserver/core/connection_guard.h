#ifndef CONNECTION_GUARD_H
#define CONNECTION_GUARD_H

#include <httpserver/core/connection_manager.h>
#include <httpserver/events/event_dispatcher.h>
#include <httpserver/events/events.h>
#include <httpserver/monitoring/log_event.h>
#include <httpserver/monitoring/logger.h>
#include <httpserver/utils/config.h>

#include <chrono>

namespace HTTPServer {

class ConnectionGuard {
 public:
  ConnectionGuard(int client_fd, const std::string& ip)
      : d_client_fd(client_fd), d_ip(ip) {
    ConnectionManager::instance().addConnection(d_ip);
    EventDispatcher::instance().dispatch(ConnectionOpenedEvent{});
    LOG_EVENT(LogLevel::INFO, LogEvent("connection_guard_enter")
                                  .add("client_fd", d_client_fd)
                                  .add("ip", d_ip));
  }

  ~ConnectionGuard() {
    ConnectionManager::instance().removeConnection(d_ip);
    EventDispatcher::instance().dispatch(ConnectionClosedEvent{});
    LOG_EVENT(LogLevel::INFO, LogEvent("connection_guard_exit")
                                  .add("client_fd", d_client_fd)
                                  .add("ip", d_ip));
  }

 private:
  int d_client_fd;
  std::string d_ip;
};

}  // namespace HTTPServer

#endif
