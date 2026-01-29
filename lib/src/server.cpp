#include "httpserver/server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <algorithm>
#include <csignal>
#include <cstring>
#include <iostream>
#include <thread>

namespace {

HTTPServer::Server* g_activeServer = nullptr;

void sig_handler(int) {
  LOG_INFO("SIGINT or SIGTERM received, shutting down ...");
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
    LOG_ERROR(prefix + ": " + buf);
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

}  // namespace

namespace HTTPServer {

Server::Server()
    : server_fd(-1),
      redirection_server_fd(-1) {
  LOG_INFO("Startup: Using default server config");
}

Server::Server(const std::string& config_path)
    : server_fd(-1),
      redirection_server_fd(-1) {
  LOG_INFO("Startup: Loading server config info...");
  try {
    Config::initFromFile(config_path);
  } catch (const std::runtime_error& err) {
    LOG_WARN("Startup: Invalid config file - loading default config:\n  => " + std::string(err.what()));
    Config::initDefault();
  }
  LOG_INFO("Startup: Successfully loaded server config.");
}

const Port& Server::port() const { return Config::get().kPort; }

void Server::stop() {
  LOG_INFO("Shutdown: Stopping server on port " + Config::get().kPort.toString() + " ...");

  if (!d_running) return;
  d_running = false;

  if (server_fd >= 0) {
    close(server_fd);
  }

  if (redirection_server_fd >= 0) {
    close(redirection_server_fd);
  }

  d_periodic_idle_ip_cleanup->stop();
  cleanup_ssl_context();
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

  ssl_ctx = SSL_CTX_new(TLS_server_method());
  if (!ssl_ctx) {
    LOG_ERROR("Startup: Fatal: Failed to create SSL context");
    return false;
  }

  if (SSL_CTX_use_certificate_file(ssl_ctx, Config::get().kCertFile.c_str(),
                                   SSL_FILETYPE_PEM) <= 0 ||
      SSL_CTX_use_PrivateKey_file(ssl_ctx, Config::get().kKeyFile.c_str(),
                                  SSL_FILETYPE_PEM) <= 0) {
    LOG_ERROR("Startup: Fatal: Failed to load certificate or key");
    return false;
  }

  if (!SSL_CTX_check_private_key(ssl_ctx)) {
    LOG_ERROR("Startup: Fatal: Private key does not match certificate");
    return false;
  }

  SSL_CTX_set_min_proto_version(ssl_ctx, TLS1_2_VERSION);
  SSL_CTX_set_options(ssl_ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 |
                                   SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1);
  SSL_CTX_set_cipher_list(ssl_ctx,
                          "ECDHE-ECDSA-AES256-GCM-SHA384:"
                          "ECDHE-RSA-AES256-GCM-SHA384:"
                          "ECDHE-ECDSA-CHACHA20-POLY1305:"
                          "ECDHE-RSA-CHACHA20-POLY1305:"
                          "ECDHE-ECDSA-AES128-GCM-SHA256:"
                          "ECDHE-RSA-AES128-GCM-SHA256");
  SSL_CTX_set_ciphersuites(ssl_ctx,
                           "TLS_AES_256_GCM_SHA384:"
                           "TLS_CHACHA20_POLY1305_SHA256:"
                           "TLS_AES_128_GCM_SHA256");
  SSL_CTX_set_options(ssl_ctx, SSL_OP_CIPHER_SERVER_PREFERENCE);
  SSL_CTX_set_options(ssl_ctx, SSL_OP_NO_COMPRESSION);
  SSL_CTX_set_session_cache_mode(ssl_ctx, SSL_SESS_CACHE_SERVER);
  SSL_CTX_set_timeout(ssl_ctx, 300);  // 5 minutes
  SSL_CTX_set_options(ssl_ctx, SSL_OP_NO_RENEGOTIATION);
  return true;
}

void Server::cleanup_ssl_context() {
  if (!ssl_ctx) return;

  SSL_CTX_free(ssl_ctx);
  ssl_ctx = nullptr;
  EVP_cleanup();
}

void Server::start() {
  LOG_INFO("Startup: Starting server on port " + Config::get().kPort.toString() + " ...");

  // 1. Set up HTTPS
  if (Config::get().kEnableHttps) {
    if (!init_ssl_context()) {
      return;
    }
    LOG_INFO("Startup: HTTPS enabled");
  }

  // 2. Start main server
  sockaddr_in6 address{};
  address.sin6_family = AF_INET6;
  address.sin6_addr = in6addr_any;
  address.sin6_port = Config::get().kPort.toNetwork();

  server_fd = create_listening_socket(reinterpret_cast<sockaddr*>(&address),
                                      sizeof(address));

  if (server_fd < 0) {
    LOG_ERROR("Startup: Fatal: Failed to create main server socket");
    return;
  }

  // 3. Start thread pool
  size_t hw = static_cast<size_t>(std::thread::hardware_concurrency());
  size_t thread_count =
      std::clamp(hw, Config::get().kMinThreads, Config::get().kMaxThreads);
  d_thread_pool = std::make_unique<ThreadPool>(thread_count);
  LOG_INFO("Thread pool started with " + std::to_string(thread_count) +
           " worker threads");

  // 4. Start HTTP -> HTTPS forwarding if enabled
  if (Config::get().kEnableHttps && Config::get().kEnableHttpRedirection) {
    if (Config::get().kPort == Config::get().kRedirectionPort) {
      LOG_WARN("Startup: Redirection port [" + Config::get().kRedirectionPort.toString() +
               "] cannot be the same as server port [" + Config::get().kPort.toString() +
               "]: Failed to start HTTP redirection");
    } else {
      if (d_thread_pool->enqueue(
              [this]() { start_http_redirect(Config::get().kRedirectionPort); }) < 0) {
        LOG_WARN(
            "Startup: Thread pool queue limit reached - cannot start "
            "redirection server");
      }
    }
  }

  // 5. Start Idle IP periodic cleanup
  d_periodic_idle_ip_cleanup = std::make_unique<PerioidIdleIpCleanup>(
      d_connected_ips, d_connected_ips_mtx);

  // 6. Allow connections
  d_running = true;

  LOG_INFO("Server running on port " + Config::get().kPort.toString() + " with fd [" +
           std::to_string(server_fd) + "] ...");
  accept_loop<sockaddr_in6>(server_fd, d_running, [this](int client_fd) {
    set_socket_timeout_option(client_fd, Config::get().kClientTimeoutSec,
                              SO_RCVTIMEO);
    set_socket_timeout_option(client_fd, Config::get().kClientTimeoutSec,
                              SO_SNDTIMEO);

    LOG_INFO("Accepted client [" + std::to_string(client_fd) + "]");
    if (d_thread_pool->enqueue(
            [this, client_fd]() { dispatch_client(client_fd); }) < 0) {
      close(client_fd);
    }
  });
  LOG_INFO("Shutdown: Server main loop exited.");
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
  {
    std::lock_guard lock(d_connected_ips_mtx);
    auto& entry = d_connected_ips[client_ip];

    if (entry.active >= Config::get().kMaxConnectionsPerIp) {
      LOG_WARN("Connection from client [" + std::to_string(client_fd) +
               "] exeeds maximum number of connections from IP address. "
               "Closing client");
      close(client_fd);
      return;
    }
  }
  ConnectionGuard guard(client_fd, d_connected_ips_mtx,
                        d_connected_ips[client_ip]);

  if (!Config::get().kEnableHttps) {
    handle_client(client_fd, client_ip);
    return;
  }

  SSL* ssl = SSL_new(ssl_ctx);
  SSL_set_fd(ssl, client_fd);

  if (SSL_accept(ssl) <= 0) {
    LOG_ERROR("Client [" + std::to_string(client_fd) +
              "] TLS handshake failed");
    log_ssl_errors("OpenSSL");
    SSL_free(ssl);
    close(client_fd);
    return;
  }
  handle_client(ssl, client_ip);
}

void Server::handle_client(int client_fd, const std::string& client_ip) {
  init_request_processor(
      client_fd, client_ip,
      [client_fd](char* buf, size_t size) {
        return recv(client_fd, buf, size, 0);
      },
      [client_fd](const char* data, size_t size) {
        return send(client_fd, data, size, 0);
      });
}

void Server::handle_client(SSL* ssl, const std::string& client_ip) {
  int client_fd = SSL_get_fd(ssl);
  init_request_processor(
      client_fd, client_ip,
      [ssl](char* buf, size_t size) { return SSL_read(ssl, buf, size); },
      [ssl](const char* data, size_t size) {
        return SSL_write(ssl, data, size);
      },
      true, ssl);
}

void Server::start_http_redirect(const Port& redirect_port) {
  LOG_INFO("Starting HTTP redirection on port " + redirect_port.toString() +
           " ...");
  sockaddr_in6 address{};
  address.sin6_family = AF_INET6;
  address.sin6_addr = in6addr_any;
  address.sin6_port = redirect_port.toNetwork();

  redirection_server_fd = create_listening_socket(
      reinterpret_cast<sockaddr*>(&address), sizeof(address));

  if (redirection_server_fd < 0) {
    LOG_ERROR("Redirection Server: Fatal: Failed to start redirect server");
    return;
  }

  LOG_INFO("HTTP -> HTTPS redirection enabled on port " +
           redirect_port.toString() + " with fd [" +
           std::to_string(redirection_server_fd) + "] ...");
  accept_loop<sockaddr_in6>(
      redirection_server_fd, d_running, [this](int client_fd) {
        set_socket_timeout_option(client_fd, Config::get().kClientTimeoutSec,
                                  SO_RCVTIMEO);
        set_socket_timeout_option(client_fd, Config::get().kClientTimeoutSec,
                                  SO_SNDTIMEO);

        LOG_INFO("Accepted client [" + std::to_string(client_fd) +
                 "] on redirect server");

        HttpParser parser;
        std::string recvBuffer;
        while (true) {
          char buffer[Config::get().kRecvBufferSize];
          int bytes = recv(client_fd, buffer, sizeof(buffer), 0);
          if (bytes <= 0) {
            if (bytes == 0)
              LOG_INFO("Redirection Server: Client [" +
                       std::to_string(client_fd) + "] closed connection");
            else if (errno == EAGAIN || errno == EWOULDBLOCK)
              LOG_INFO("Redirection Server: Client [" +
                       std::to_string(client_fd) +
                       "] idle timeout reached, closing");
            else
              LOG_INFO("Redirection Server: Fatal: Client [" +
                       std::to_string(client_fd) + "] recv error");

            close(client_fd);
            return;
          }

          recvBuffer.append(buffer, bytes);

          std::string_view view(recvBuffer);
          ParseResult result = parser.parse(view);

          // remove consumed bytes
          recvBuffer.erase(0, recvBuffer.size() - view.size());

          if (result == ParseResult::NEED_MORE_DATA) {
            continue;
          }

          if (result == ParseResult::PARSE_ERROR) {
            ParseError err = parser.error();
            StatusCode status = parseErrorToStatusCode(err);
            LOG_ERROR("Bad HTTP request from client [" +
                      std::to_string(client_fd) + "] with parse error " +
                      std::to_string(static_cast<int>(err)));
            HttpResponse response = Responses::badRequest(status);
            std::string payload = response.serialize();
            send(client_fd, payload.c_str(), payload.size(), 0);
            close(client_fd);
            return;
          }

          if (result == ParseResult::REQUEST_COMPLETE) {
            HttpRequest request = parser.takeRequest();

            HttpResponse response = Responses::redirection(request, Config::get().kPort);

            std::string payload = response.serialize();
            send(client_fd, payload.c_str(), payload.size(), 0);
            parser.reset();

            close(client_fd);

            LOG_INFO("Client [" + std::to_string(client_fd) +
                     "] redirected and disconnected");
            return;
          }
        }
      });
  LOG_INFO("Shutdown: HTTP -> HTTPS redirection stopped.");
}

}  // namespace HTTPServer