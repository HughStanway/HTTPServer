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
#include "periodic_idle_ip_cleanup.h"
#include "port.h"
#include "router.h"
#include "thread_pool.h"
#include "utils.h"

namespace {

bool is_tls_handshake_attempt(const std::string& data) {
  // Detects TLS 1.0, 1.1, 1.2, and 1.3 handshake attempts
  // TLS record format: content_type (1 byte) + version (2 bytes) + length (2
  // bytes) + payload All TLS versions use:
  //   - Content type 0x16 (Handshake)
  //   - Version bytes 0x03 0x0x (0x03 0x01 for TLS 1.0, 0x03 0x02 for TLS 1.1,
  //   etc.)
  //   - TLS 1.3 uses 0x03 0x03 in the record (legacy compatibility)

  if (data.size() < 3) {
    return false;
  }

  unsigned char byte0 = static_cast<unsigned char>(data[0]);
  unsigned char byte1 = static_cast<unsigned char>(data[1]);
  unsigned char byte2 = static_cast<unsigned char>(data[2]);

  // Check for TLS handshake record (0x16)
  if (byte0 != 0x16) {
    return false;
  }

  // Check for TLS version (0x03 0x0x where second byte is 0x01-0x03)
  // TLS 1.0: 0x03 0x01
  // TLS 1.1: 0x03 0x02
  // TLS 1.2: 0x03 0x03
  // TLS 1.3: 0x03 0x03 (uses legacy version in record)
  if (byte1 == 0x03 && byte2 >= 0x01 && byte2 <= 0x03) {
    return true;
  }

  return false;
}

}  // namespace

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

  template <typename Reader, typename Writer>
  void init_request_processor(int client_fd, const std::string& client_ip,
                              Reader readFunc, Writer writeFunc,
                              bool isTLS = false, SSL* ssl = nullptr);
  template <typename Address, typename Handler>
  void accept_loop(int listen_fd, std::atomic<bool>& running, Handler handler);
  bool init_ssl_context();
  void cleanup_ssl_context();
  void dispatch_client(int client_fd);
  void handle_client(SSL* ssl, const std::string& client_ip);
  void handle_client(int client_fd, const std::string& client_ip);
  void start_http_redirect(const Port& redirection_port);
  void refill_tokens(ConnectedIp& client_ip);
  bool consume_token(ConnectedIp& client_ip);
  bool allow_request_from_ip(const std::string& ip);
};

inline void Server::refill_tokens(ConnectedIp& client_ip) {
  auto now = std::chrono::steady_clock::now();
  std::chrono::duration<double> elapsed = now - client_ip.last_seen;

  client_ip.tokens =
      std::min(Config::get().kMaxTokens,
               client_ip.tokens + elapsed.count() * Config::get().kRefillRate);
  client_ip.last_seen = now;
}

inline bool Server::consume_token(ConnectedIp& client_ip) {
  if (client_ip.tokens < 1.0) {
    return false;
  }
  client_ip.tokens -= 1.0;
  return true;
}

inline bool Server::allow_request_from_ip(const std::string& ip) {
  std::lock_guard lock(d_connected_ips_mtx);
  auto& entry = d_connected_ips[ip];
  refill_tokens(entry);

  if (!consume_token(entry)) {
    return false;
  }
  return true;
}

template <typename Reader, typename Writer>
void Server::init_request_processor(int client_fd, const std::string& client_ip,
                                    Reader readFunc, Writer writeFunc,
                                    bool isTLS, SSL* ssl) {
  LOG_EVENT(LogLevel::INFO, LogEvent("connected")
                                .add("client_fd", client_fd)
                                .add("ip", client_ip)
                                .add("tls", isTLS));

  HttpParser parser;
  std::string recvBuffer;
  bool keepAlive = true;
  int requests_handled = 0;
  while (keepAlive) {
    auto request_start = std::chrono::steady_clock::now();
    char buffer[Config::get().kRecvBufferSize];
    int bytes = readFunc(buffer, sizeof(buffer));
    if (bytes <= 0) {
      if (bytes == 0)
        LOG_EVENT(LogLevel::INFO, LogEvent("client_closed_connection")
                                      .add("client_fd", client_fd)
                                      .add("ip", client_ip)
                                      .add("tls", isTLS));
      else if (errno == EAGAIN || errno == EWOULDBLOCK)
        LOG_EVENT(LogLevel::INFO, LogEvent("client_idle_timeout")
                                      .add("client_fd", client_fd)
                                      .add("ip", client_ip)
                                      .add("tls", isTLS));
      else
        LOG_EVENT(LogLevel::ERROR, LogEvent("recv_error")
                                       .add("client_fd", client_fd)
                                       .add("ip", client_ip)
                                       .add("tls", isTLS));
      break;
    }

    recvBuffer.append(buffer, bytes);

    auto now = std::chrono::steady_clock::now();
    if (now - request_start >
        std::chrono::seconds(Config::get().kMaxRequestDurationSec)) {
      LOG_EVENT(LogLevel::ERROR, LogEvent("request_timeout")
                                     .add("client_fd", client_fd)
                                     .add("ip", client_ip)
                                     .add("tls", isTLS));
      HttpResponse resp = Responses::badRequest(StatusCode::RequestTimeout);
      auto payload = resp.serialize();
      writeFunc(payload.c_str(), payload.size());
      break;
    }

    // Detect TLS handshake on non-HTTPS connection
    if (!isTLS && is_tls_handshake_attempt(recvBuffer)) {
      LOG_EVENT(LogLevel::ERROR,
                LogEvent("client_attempted_tls_handshake_on_non_https_port")
                    .add("client_fd", client_fd)
                    .add("ip", client_ip)
                    .add("tls", isTLS));
      break;
    }

    std::string_view view(recvBuffer);
    ParseResult result = parser.parse(view);

    // Remove consumed bytes
    recvBuffer.erase(0, recvBuffer.size() - view.size());

    if (result == ParseResult::NEED_MORE_DATA) {
      continue;
    }

    if (result == ParseResult::PARSE_ERROR) {
      ParseError err = parser.error();
      StatusCode status = parseErrorToStatusCode(err);
      LOG_EVENT(LogLevel::ERROR,
                LogEvent("bad_request")
                    .add("client_fd", client_fd)
                    .add("ip", client_ip)
                    .add("tls", isTLS)
                    .add("parse_error", static_cast<int>(err)));
      HttpResponse resp = Responses::badRequest(status);
      auto payload = resp.serialize();
      writeFunc(payload.c_str(), payload.size());
      break;
    }

    // REQUEST_COMPLETE
    HttpRequest request = parser.takeRequest();
    LOG_EVENT(LogLevel::INFO, LogEvent("http_request")
                                  .add("client_fd", client_fd)
                                  .add("ip", client_ip)
                                  .add("tls", isTLS)
                                  .add("method", request.method)
                                  .add("path", request.path));

    if (requests_handled >= Config::get().kMaxKeepAliveRequests) {
      LOG_EVENT(LogLevel::WARN, LogEvent("max_keep_alive_requests_exceeded")
                                    .add("client_fd", client_fd)
                                    .add("ip", client_ip)
                                    .add("tls", isTLS)
                                    .add("max_keep_alive_requests",
                                         Config::get().kMaxKeepAliveRequests));
      HttpResponse resp = Responses::badRequest(StatusCode::TooManyRequests);
      auto payload = resp.serialize();
      writeFunc(payload.c_str(), payload.size());
      break;
    }

    if (!allow_request_from_ip(client_ip)) {
      LOG_EVENT(LogLevel::WARN, LogEvent("rate_limit_exceeded")
                                    .add("client_fd", client_fd)
                                    .add("ip", client_ip)
                                    .add("tls", isTLS));
      HttpResponse resp = Responses::badRequest(StatusCode::TooManyRequests);
      auto payload = resp.serialize();
      writeFunc(payload.c_str(), payload.size());
      break;
    }

    HttpResponse response = Router::instance().route(request);
    keepAlive = requestWantsKeepAlive(request);

    std::string payload = response.serialize();
    bytes = writeFunc(payload.c_str(), payload.size());
    if (bytes <= 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        LOG_EVENT(LogLevel::ERROR, LogEvent("client_send_timeout")
                                       .add("client_fd", client_fd)
                                       .add("ip", client_ip)
                                       .add("tls", isTLS));
      }
    }

    parser.reset();
    requests_handled++;
  }

  if (isTLS && ssl) {
    SSL_shutdown(ssl);
    SSL_free(ssl);
  }

  close(client_fd);
  LOG_EVENT(LogLevel::INFO, LogEvent("client_disconnected")
                                .add("client_fd", client_fd)
                                .add("ip", client_ip)
                                .add("tls", isTLS));
}

}  // namespace HTTPServer

#endif