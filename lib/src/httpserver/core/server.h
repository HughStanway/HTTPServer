#ifndef SERVER_H
#define SERVER_H

#include <httpserver/core/connection_manager.h>
#include <httpserver/http/http_object.h>
#include <httpserver/http/http_parser.h>
#include <httpserver/http/http_response_builder.h>
#include <httpserver/monitoring/log_event.h>
#include <httpserver/monitoring/logger.h>
#include <httpserver/monitoring/metrics.h>
#include <httpserver/routing/router.h>
#include <httpserver/threading/thread_pool.h>
#include <httpserver/utils/config.h>
#include <httpserver/utils/periodic_idle_ip_cleanup.h>
#include <httpserver/utils/port.h>
#include <httpserver/utils/unique_fd.h>
#include <httpserver/utils/utils.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <unistd.h>

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

namespace HTTPServer {

class Server {
  using UniqueSSL_CTX = std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)>;

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
  UniqueFd server_fd;
  UniqueFd redirection_server_fd;
  std::atomic<bool> d_running{false};
  std::unique_ptr<ThreadPool> d_thread_pool;
  UniqueSSL_CTX ssl_ctx{nullptr, SSL_CTX_free};
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