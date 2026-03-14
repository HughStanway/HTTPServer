#include <arpa/inet.h>
#include <httpserver_impl/core/connection_handler.h>
#include <httpserver_impl/core/server.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <algorithm>
#include <csignal>
#include <cstring>
#include <iostream>
#include <thread>

namespace {

HTTPServer::Server* g_activeServer = nullptr;

void sig_handler(int signal) {
  LOG_EVENT(
      HTTPServer::LogLevel::INFO,
      HTTPServer::LogEvent("shutdown_signal_received").add("signal", signal));
  if (g_activeServer) {
    g_activeServer->stop();
  }
}

int create_listening_socket(const sockaddr* addr, socklen_t addrlen,
                            bool dualStackIPv6 = true) {
  int fd = socket(AF_INET6, SOCK_STREAM, 0);
  if (fd < 0) {
    LOG_ERROR_ERRNO("Socket creation failed");
    return -1;
  }

  int opt = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    LOG_ERROR_ERRNO("setsockopt(SO_REUSEADDR) failed");
  }

  if (dualStackIPv6) {
    int off = 0;
    setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));
  }

  if (bind(fd, addr, addrlen) < 0) {
    LOG_ERROR_ERRNO("Bind failed");
    close(fd);
    return -1;
  }

  if (listen(fd, SOMAXCONN) < 0) {
    LOG_ERROR_ERRNO("Listen failed");
    close(fd);
    return -1;
  }

  return fd;
}

template <typename Option>
void set_socket_timeout_option(int fd, int seconds, Option option) {
  struct timeval timeout{};
  timeout.tv_sec = seconds;
  timeout.tv_usec = 0;

  if (setsockopt(fd, SOL_SOCKET, option, &timeout, sizeof(timeout)) < 0) {
    LOG_ERROR_ERRNO("setsockopt(SO_RCVTIMEO) failed");
  }
}

void log_ssl_errors(const std::string& prefix) {
  unsigned long err;
  while ((err = ERR_get_error()) != 0) {
    char buf[256];
    ERR_error_string_n(err, buf, sizeof(buf));
    LOG_EVENT(HTTPServer::LogLevel::ERROR,
              HTTPServer::LogEvent("ssl_error")
                  .add("source", prefix)
                  .add("error_code", std::to_string(err))
                  .add("error_message", std::string(buf)));
  }
}

std::string extract_ip(int fd) {
  sockaddr_storage addr{};
  socklen_t len = sizeof(addr);

  if (getpeername(fd, reinterpret_cast<sockaddr*>(&addr), &len) < 0)
    return "unknown";

  char buf[INET6_ADDRSTRLEN]{};

  if (addr.ss_family == AF_INET) {
    inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(&addr)->sin_addr, buf,
              sizeof(buf));
  } else {
    inet_ntop(AF_INET6, &reinterpret_cast<sockaddr_in6*>(&addr)->sin6_addr, buf,
              sizeof(buf));
  }

  return buf;
}

void send_bad_request_and_close(HTTPServer::StatusCode code, int client_fd) {
  HTTPServer::HttpResponse response = HTTPServer::Responses::badRequest(code);
  const std::string payload = response.serialize();
  send(client_fd, payload.c_str(), payload.size(), 0);
  HTTPServer::Metrics::instance().recordResponseStatus(response.code);
  close(client_fd);
}

}  // namespace

namespace HTTPServer {

Server::Server() {
  LOG_EVENT(LogLevel::INFO, LogEvent("startup_default_config"));
}

Server::Server(const std::string& config_path) {
  LOG_EVENT(LogLevel::INFO,
            LogEvent("startup_loading_config").add("config_path", config_path));
  try {
    Config::initFromFile(config_path);
  } catch (const std::runtime_error& err) {
    LOG_EVENT(LogLevel::WARN, LogEvent("startup_invalid_config")
                                  .add("config_path", config_path)
                                  .add("error", std::string(err.what())));
    Config::initDefault();
  }
  LOG_EVENT(LogLevel::INFO,
            LogEvent("startup_config_loaded").add("config_path", config_path));
}

const Port& Server::port() const { return Config::get().kPort; }

void Server::stop() {
  LOG_EVENT(
      LogLevel::INFO,
      LogEvent("server_stopping").add("port", Config::get().kPort.toString()));

  if (!d_running) return;
  d_running = false;

  if (server_fd.is_valid()) {
    server_fd.reset();
  }

  if (redirection_server_fd.is_valid()) {
    redirection_server_fd.reset();
  }

  d_periodic_idle_ip_cleanup->stop();
  ssl_ctx.reset();
  d_thread_pool->stop();
}

void Server::installSignalHandlers() {
  g_activeServer = this;
  std::signal(SIGPIPE, SIG_IGN);
  std::signal(SIGINT, sig_handler);
  std::signal(SIGTERM, sig_handler);
}

bool Server::init_ssl_context() {
  SSL_load_error_strings();
  OpenSSL_add_ssl_algorithms();

  ssl_ctx.reset(SSL_CTX_new(TLS_server_method()));
  if (!ssl_ctx) {
    LOG_EVENT(LogLevel::ERROR, LogEvent("ssl_context_create_failed"));
    return false;
  }

  if (SSL_CTX_use_certificate_file(ssl_ctx.get(),
                                   Config::get().kCertFile.c_str(),
                                   SSL_FILETYPE_PEM) <= 0 ||
      SSL_CTX_use_PrivateKey_file(ssl_ctx.get(), Config::get().kKeyFile.c_str(),
                                  SSL_FILETYPE_PEM) <= 0) {
    LOG_EVENT(LogLevel::ERROR, LogEvent("ssl_cert_or_key_load_failed")
                                   .add("cert_file", Config::get().kCertFile)
                                   .add("key_file", Config::get().kKeyFile));
    return false;
  }

  if (!SSL_CTX_check_private_key(ssl_ctx.get())) {
    LOG_EVENT(LogLevel::ERROR, LogEvent("ssl_private_key_mismatch")
                                   .add("cert_file", Config::get().kCertFile)
                                   .add("key_file", Config::get().kKeyFile));
    return false;
  }

  SSL_CTX_set_min_proto_version(ssl_ctx.get(), TLS1_2_VERSION);
  SSL_CTX_set_options(ssl_ctx.get(), SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 |
                                         SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1);
  SSL_CTX_set_cipher_list(ssl_ctx.get(),
                          "ECDHE-ECDSA-AES256-GCM-SHA384:"
                          "ECDHE-RSA-AES256-GCM-SHA384:"
                          "ECDHE-ECDSA-CHACHA20-POLY1305:"
                          "ECDHE-RSA-CHACHA20-POLY1305:"
                          "ECDHE-ECDSA-AES128-GCM-SHA256:"
                          "ECDHE-RSA-AES128-GCM-SHA256");
  SSL_CTX_set_ciphersuites(ssl_ctx.get(),
                           "TLS_AES_256_GCM_SHA384:"
                           "TLS_CHACHA20_POLY1305_SHA256:"
                           "TLS_AES_128_GCM_SHA256");
  SSL_CTX_set_options(ssl_ctx.get(), SSL_OP_CIPHER_SERVER_PREFERENCE);
  SSL_CTX_set_options(ssl_ctx.get(), SSL_OP_NO_COMPRESSION);
  SSL_CTX_set_session_cache_mode(ssl_ctx.get(), SSL_SESS_CACHE_SERVER);
  SSL_CTX_set_timeout(ssl_ctx.get(), 300);  // 5 minutes
  SSL_CTX_set_options(ssl_ctx.get(), SSL_OP_NO_RENEGOTIATION);
  return true;
}

void Server::start() {
  LOG_EVENT(
      LogLevel::INFO,
      LogEvent("server_starting").add("port", Config::get().kPort.toString()));

  // 1. Set up HTTPS
  if (Config::get().kEnableHttps) {
    if (!init_ssl_context()) {
      return;
    }
    LOG_EVENT(LogLevel::INFO, LogEvent("https_enabled"));
  }

  // 2. Start main server
  sockaddr_in6 address{};
  address.sin6_family = AF_INET6;
  address.sin6_addr = in6addr_any;
  address.sin6_port = Config::get().kPort.toNetwork();

  server_fd.reset(create_listening_socket(reinterpret_cast<sockaddr*>(&address),
                                          sizeof(address)));

  if (!server_fd.is_valid()) {
    LOG_EVENT(LogLevel::ERROR,
              LogEvent("main_socket_create_failed")
                  .add("port", Config::get().kPort.toString()));
    return;
  }

  // 3. Start thread pool
  size_t hw = static_cast<size_t>(std::thread::hardware_concurrency());
  size_t thread_count =
      std::clamp(hw, Config::get().kMinThreads, Config::get().kMaxThreads);
  d_thread_pool = std::make_unique<ThreadPool>(thread_count);
  LOG_EVENT(
      LogLevel::INFO,
      LogEvent("thread_pool_started").add("worker_threads", thread_count));

  // 4. Start HTTP -> HTTPS forwarding if enabled
  if (Config::get().kEnableHttps && Config::get().kEnableHttpRedirection) {
    if (Config::get().kPort == Config::get().kRedirectionPort) {
      LOG_EVENT(LogLevel::WARN,
                LogEvent("redirection_port_conflict")
                    .add("redirection_port",
                         Config::get().kRedirectionPort.toString())
                    .add("server_port", Config::get().kPort.toString()));
    } else {
      if (d_thread_pool->enqueue([this]() {
            start_http_redirect(Config::get().kRedirectionPort);
          }) < 0) {
        LOG_EVENT(LogLevel::WARN,
                  LogEvent("redirection_start_queue_limit_reached")
                      .add("redirection_port",
                           Config::get().kRedirectionPort.toString()));
      }
    }
  }

  // 5. Start Idle IP periodic cleanup
  d_periodic_idle_ip_cleanup = std::make_unique<PerioidIdleIpCleanup>();

  // 6. Allow connections
  d_running = true;

  LOG_EVENT(LogLevel::INFO, LogEvent("server_running")
                                .add("port", Config::get().kPort.toString())
                                .add("fd", server_fd.get()));
  accept_loop<sockaddr_in6>(server_fd.get(), d_running, [this](int client_fd) {
    set_socket_timeout_option(client_fd, Config::get().kClientTimeoutSec,
                              SO_RCVTIMEO);
    set_socket_timeout_option(client_fd, Config::get().kClientTimeoutSec,
                              SO_SNDTIMEO);

    LOG_EVENT(LogLevel::INFO,
              LogEvent("connection_accepted").add("client_fd", client_fd));
    if (d_thread_pool->enqueue(
            [this, client_fd]() { dispatch_client(client_fd); }) < 0) {
      send_bad_request_and_close(StatusCode::ServiceUnavailable, client_fd);
    }
  });
  LOG_EVENT(LogLevel::INFO, LogEvent("server_main_loop_exited"));
}

template <typename Address, typename Handler>
void Server::accept_loop(int listen_fd, std::atomic<bool>& running,
                         Handler handler) {
  while (running) {
    Address client_addr{};
    socklen_t addrlen = sizeof(client_addr);

    int client_fd =
        accept(listen_fd, reinterpret_cast<sockaddr*>(&client_addr), &addrlen);

    if (client_fd < 0) {
      if (!running || errno == EBADF || errno == EINVAL) break;
      LOG_ERROR_ERRNO("Incoming connection accept failed");
      continue;
    }

    handler(client_fd);
  }
}

void Server::dispatch_client(int client_fd) {
  std::string client_ip = extract_ip(client_fd);

  if (!ConnectionManager::instance().canAcceptConnection(client_ip)) {
    send_bad_request_and_close(StatusCode::ServiceUnavailable, client_fd);
    return;
  }

  ConnectionGuard guard(client_fd, client_ip);

  if (!Config::get().kEnableHttps) {
    handle_client(client_fd, client_ip);
    return;
  }

  SSL* ssl = SSL_new(ssl_ctx.get());
  SSL_set_fd(ssl, client_fd);

  if (SSL_accept(ssl) <= 0) {
    LOG_EVENT(LogLevel::ERROR, LogEvent("tls_handshake_failed")
                                   .add("client_fd", client_fd)
                                   .add("ip", client_ip));
    log_ssl_errors("OpenSSL");
    SSL_free(ssl);
    close(client_fd);
    return;
  }
  handle_client(ssl, client_ip);
}

void Server::handle_client(int client_fd, const std::string& client_ip) {
  ConnectionHandler handler(
      client_fd, client_ip, false, nullptr,
      [client_fd](char* buf, size_t size) {
        return recv(client_fd, buf, size, 0);
      },
      [client_fd](const char* data, size_t size) {
        return send(client_fd, data, size, 0);
      });
  handler.process();
}

void Server::handle_client(SSL* ssl, const std::string& client_ip) {
  int client_fd = SSL_get_fd(ssl);
  ConnectionHandler handler(
      client_fd, client_ip, true, ssl,
      [ssl](char* buf, size_t size) { return SSL_read(ssl, buf, size); },
      [ssl](const char* data, size_t size) {
        return SSL_write(ssl, data, size);
      });
  handler.process();
}

void Server::start_http_redirect(const Port& redirect_port) {
  LOG_EVENT(LogLevel::INFO, LogEvent("redirection_server_starting")
                                .add("port", redirect_port.toString()));
  sockaddr_in6 address{};
  address.sin6_family = AF_INET6;
  address.sin6_addr = in6addr_any;
  address.sin6_port = redirect_port.toNetwork();

  redirection_server_fd.reset(create_listening_socket(
      reinterpret_cast<sockaddr*>(&address), sizeof(address)));

  if (!redirection_server_fd.is_valid()) {
    LOG_EVENT(LogLevel::ERROR, LogEvent("redirection_server_start_failed")
                                   .add("port", redirect_port.toString()));
    return;
  }

  LOG_EVENT(LogLevel::INFO, LogEvent("redirection_server_running")
                                .add("port", redirect_port.toString())
                                .add("fd", redirection_server_fd.get()));
  accept_loop<sockaddr_in6>(
      redirection_server_fd.get(), d_running, [this](int client_fd) {
        std::string client_ip = extract_ip(client_fd);

        set_socket_timeout_option(client_fd, Config::get().kClientTimeoutSec,
                                  SO_RCVTIMEO);
        set_socket_timeout_option(client_fd, Config::get().kClientTimeoutSec,
                                  SO_SNDTIMEO);

        LOG_EVENT(LogLevel::INFO, LogEvent("connection_accepted")
                                      .add("client_fd", client_fd)
                                      .add("redirection_server", true));

        auto readFunc = [client_fd](char* buf, size_t size) {
          return recv(client_fd, buf, size, 0);
        };
        auto writeFunc = [client_fd](const char* data, size_t size) {
          return send(client_fd, data, size, 0);
        };

        ConnectionHandler handler(client_fd, client_ip, false, nullptr,
                                  readFunc, writeFunc, true);
        handler.process();
      });
  LOG_EVENT(LogLevel::INFO, LogEvent("redirection_server_stopped")
                                .add("port", redirect_port.toString()));
}

}  // namespace HTTPServer