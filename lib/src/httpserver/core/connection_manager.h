#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

#include <httpserver/utils/config.h>

#include <mutex>
#include <string>
#include <unordered_map>

namespace HTTPServer {

struct TokenBucket {
  double tokens = Config::get().kMaxTokens;
  std::chrono::steady_clock::time_point last_seen =
      std::chrono::steady_clock::now();
};

struct ConnectedIp {
  int active = 0;
  TokenBucket tokenBucket;
};

class ConnectionManager {
 public:
  static ConnectionManager& instance();
  bool canAcceptConnection(const std::string& ip);
  void addConnection(const std::string& ip);
  void removeConnection(const std::string& ip);
  bool allowRequest(const std::string& ip);
  void removeIdleConnections();

 private:
  ConnectionManager() = default;
  ~ConnectionManager() = default;
  ConnectionManager(const ConnectionManager&) = delete;
  ConnectionManager& operator=(const ConnectionManager&) = delete;

  std::mutex d_mtx;
  std::unordered_map<std::string, ConnectedIp> d_ips;
};

}  // namespace HTTPServer

#endif