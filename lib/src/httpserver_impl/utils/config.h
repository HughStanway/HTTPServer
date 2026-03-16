#ifndef CONFIG_H
#define CONFIG_H

#include <httpserver_impl/monitoring/log_level.h>
#include <httpserver_impl/monitoring/logger.h>
#include <httpserver_impl/utils/port.h>

#include <chrono>
#include <string>
#include <unordered_set>

namespace HTTPServer {

struct ServerConfig {
  /**
   * Server Ports
   */
  Port kPort{443};
  Port kRedirectionPort{80};

  /**
   * Https
   */
  bool kEnableHttps = false;
  std::string kCertFile{};
  std::string kKeyFile{};
  bool kEnableHttpRedirection = false;

  /**
   * Periodic Idle IP Cleanup
   */
  std::chrono::seconds kCleanupInterval{600};
  std::chrono::seconds kIdleTimeout{300};

  /**
   * Networking
   */
  int kClientTimeoutSec = 5;
  int kMaxRequestDurationSec = 30;
  int kRecvBufferSize = 4096;

  /**
   * Threading
   */
  size_t kMinThreads = 4;
  size_t kMaxThreads = 32;
  int kMaxQueueSize = 1024;

  /**
   * Rate Limiting
   */
  int kMaxConnectionsPerIp = 10;
  int kMaxKeepAliveRequests = 100;
  double kMaxTokens = 10.0;
  double kRefillRate = 5.0;

  /**
   * HTTP Limits
   */
  int kMaxHeaderBytes = 16 * 1024;
  int kMaxHeaderCount = 100;
  int kMaxTotalHeaderSize = 64 * 1024;
  int kMaxBodyBytes = 10 * 1024 * 1024;
  std::unordered_set<std::string> kAllowedMethods = {
      "GET",
      "POST",
  };

  std::unordered_set<std::string> kAllowedVersions = {
      "HTTP/1.0",
      "HTTP/1.1",
  };

  /**
   * Logging level
   */
  LogLevel kLogLevel{LogLevel::INFO};
  bool kFileLoggingEnabled = false;
  std::string kFileLoggingPath{};
};

class Config {
 public:
  static void initDefault();
  static void initFromFile(const std::string& path);
  static const ServerConfig& get();

 private:
  static ServerConfig d_config;
};

}  // namespace HTTPServer

#endif