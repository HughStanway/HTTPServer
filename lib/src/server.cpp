#include "httpserver/server.h"

#include <netinet/in.h>
#include <sys/socket.h>

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

template <typename Address, typename Handler>
void accept_loop(int listen_fd, std::atomic<bool>& running, Handler handler) {
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

void set_socket_recv_timeout(int fd, int seconds) {
  struct timeval timeout{};
  timeout.tv_sec = seconds;
  timeout.tv_usec = 0;

  if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
    LOG_ERROR_ERRNO("setsockopt(SO_RCVTIMEO) failed");
  }
}

}  // namespace

namespace HTTPServer {

Server::Server(Port port)
    : d_port(port),
      server_fd(-1),
      d_redirection_port(Port(kDefaultHttpRedirectPort)),
      redirection_server_fd(-1) {}

const Port& Server::port() const { return d_port; }

void Server::stop() {
  LOG_INFO("Shutdown: Stopping server on port " + d_port.toString() + " ...");

  if (!d_running) return;
  d_running = false;

  if (server_fd >= 0) {
    close(server_fd);
  }

  if (!(redirection_server_fd < 0)) {
    close(redirection_server_fd);
  }

  cleanup_ssl_context();

  // Join all client threads
  for (auto& t : client_threads) {
    if (t.joinable()) t.join();
  }
  client_threads.clear();
  LOG_INFO("Shutdown: All client threads finished.");
}

void Server::installSignalHandlers() {
  g_activeServer = this;
  std::signal(SIGPIPE, SIG_IGN);
  std::signal(SIGINT, sig_handler);
  std::signal(SIGTERM, sig_handler);
}

void Server::enableHttps(const std::string& certFile,
                         const std::string& keyFile) {
  https_enabled = true;
  cert_path = certFile;
  key_path = keyFile;
}

void Server::enableHttpRedirection(Port redirection_port) {
  http_redirection_enabled = true;
  d_redirection_port = redirection_port;
}

bool Server::init_ssl_context() {
  SSL_load_error_strings();
  OpenSSL_add_ssl_algorithms();

  ssl_ctx = SSL_CTX_new(TLS_server_method());
  if (!ssl_ctx) {
    LOG_ERROR("Startup: Fatal: Failed to create SSL context");
    return false;
  }

  if (SSL_CTX_use_certificate_file(ssl_ctx, cert_path.c_str(),
                                   SSL_FILETYPE_PEM) <= 0 ||
      SSL_CTX_use_PrivateKey_file(ssl_ctx, key_path.c_str(),
                                  SSL_FILETYPE_PEM) <= 0) {
    LOG_ERROR("Startup: Fatal: Failed to load certificate or key");
    return false;
  }

  SSL_CTX_set_min_proto_version(ssl_ctx, TLS1_2_VERSION);
  return true;
}

void Server::cleanup_ssl_context() {
  if (!ssl_ctx) return;

  SSL_CTX_free(ssl_ctx);
  ssl_ctx = nullptr;
  EVP_cleanup();
}

void Server::start() {
  LOG_INFO("Starting server on port " + d_port.toString() + " ...");

  // 1. Set up HTTPS
  if (https_enabled) {
    if (!init_ssl_context()) {
      return;
    }
    LOG_INFO("Startup: HTTPS enabled");
  }

  // 2. Start main server
  sockaddr_in6 address{};
  address.sin6_family = AF_INET6;
  address.sin6_addr = in6addr_any;
  address.sin6_port = d_port.toNetwork();

  server_fd = create_listening_socket(reinterpret_cast<sockaddr*>(&address),
                                      sizeof(address));

  if (server_fd < 0) {
    LOG_ERROR("Startup: Fatal: Failed to create main server socket");
    return;
  }
  d_running = true;

  // 3. Start HTTP -> HTTPS forwarding if enabled
  if (https_enabled && http_redirection_enabled) {
    if (d_port == d_redirection_port) {
      LOG_WARN("Startup: Redirection port [" + d_redirection_port.toString() +
               "] cannot be the same as server port [" + d_port.toString() +
               "]: Failed to start HTTP redirection");
    } else {
      client_threads.emplace_back(
          [this]() { start_http_redirect(d_redirection_port); });
    }
  }

  LOG_INFO("Server running on port " + d_port.toString() + " with fd [" +
           std::to_string(server_fd) + "] ...");
  accept_loop<sockaddr_in>(server_fd, d_running, [this](int client_fd) {
    set_socket_recv_timeout(client_fd, kClientRecvTimeoutSec);

    LOG_INFO("Accepted client [" + std::to_string(client_fd) + "]");
    dispatch_client(client_fd);
  });
  LOG_INFO("Shutdown: Server main loop exited.");
}

void Server::dispatch_client(int client_fd) {
  if (!https_enabled) {
    client_threads.emplace_back(
        [this, client_fd]() { handle_client(client_fd); });
    return;
  }

  SSL* ssl = SSL_new(ssl_ctx);
  SSL_set_fd(ssl, client_fd);

  if (SSL_accept(ssl) <= 0) {
    LOG_ERROR("Client [" + std::to_string(client_fd) +
              "] TLS handshake failed");
    SSL_free(ssl);
    close(client_fd);
    return;
  }

  client_threads.emplace_back([this, ssl]() { handle_client(ssl); });
}

void Server::handle_client(int client_fd) {
  init_request_processor(
      client_fd,
      [client_fd](char* buf, size_t size) {
        return recv(client_fd, buf, size, 0);
      },
      [client_fd](const char* data, size_t size) {
        return send(client_fd, data, size, 0);
      });
}

void Server::handle_client(SSL* ssl) {
  int client_fd = SSL_get_fd(ssl);
  init_request_processor(
      client_fd,
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
        set_socket_recv_timeout(client_fd, kClientRecvTimeoutSec);

        LOG_INFO("Accepted client [" + std::to_string(client_fd) +
                 "] on redirect server");

        char buffer[kRecvBufferSize];
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

        std::string raw(buffer, bytes);
        HttpRequest request;
        HttpParser::parse(raw, request);

        HttpResponse response = Responses::redirection(request, d_port);

        std::string payload = response.serialize();
        send(client_fd, payload.c_str(), payload.size(), 0);
        close(client_fd);

        LOG_INFO("Client [" + std::to_string(client_fd) +
                 "] disconnected from redirect server");
      });
  LOG_INFO("Shutdown: HTTP -> HTTPS redirection stopped.");
}

}  // namespace HTTPServer