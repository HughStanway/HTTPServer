#ifndef SERVER_H
#define SERVER_H

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <unistd.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "config.h"
#include "connection_guard.h"
#include "http_object.h"
#include "http_parser.h"
#include "http_response_builder.h"
#include "log_event.h"
#include "logger.h"
#include "metrics.h"
#include "periodic_idle_ip_cleanup.h"
#include "port.h"
#include "router.h"
#include "thread_pool.h"
#include "utils.h"

namespace HTTPServer {

class Server {
 public:
  explicit Server();
  explicit Server(const std::string& config_path);
  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;

  const Port& port() const;
  void start();
  void installSignalHandlers();
  void stop();

 private:
  int server_fd{-1};
  int redirection_server_fd{-1};
  std::atomic<bool> d_running{false};
  std::unique_ptr<ThreadPool> d_thread_pool;
  SSL_CTX* ssl_ctx{nullptr};
  std::mutex d_connected_ips_mtx;
  std::unordered_map<std::string, ConnectedIp> d_connected_ips;
  std::unique_ptr<PerioidIdleIpCleanup> d_periodic_idle_ip_cleanup;

  template <typename Address, typename Handler>
  void accept_loop(int listen_fd, std::atomic<bool>& running, Handler handler);
  bool init_ssl_context();
  void cleanup_ssl_context();
  void dispatch_client(int client_fd);
  void handle_client(SSL* ssl, const std::string& client_ip);
  void handle_client(int client_fd, const std::string& client_ip);
  void start_http_redirect(const Port& redirection_port);
};

}  // namespace HTTPServer

#endif