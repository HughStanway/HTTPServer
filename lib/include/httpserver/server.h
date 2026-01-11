#ifndef SERVER_H
#define SERVER_H

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <unistd.h>

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#include "httpserver/http_object.h"
#include "httpserver/http_parser.h"
#include "httpserver/http_response_builder.h"
#include "httpserver/logger.h"
#include "httpserver/port.h"
#include "httpserver/router.h"
#include "httpserver/thread_pool.h"
#include "httpserver/utils.h"

namespace HTTPServer {

class Server {
 public:
  explicit Server(Port port = Port(443));
  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;

  const Port& port() const;
  void start();
  void installSignalHandlers();
  void stop();
  void enableHttps(const std::string& certFile, const std::string& keyFile);
  void enableHttpRedirection(Port redirection_port = Port(80));

 private:
  static constexpr int kClientTimeoutSec = 5;
  static constexpr int kDefaultHttpRedirectPort = 8080;
  static constexpr size_t kRecvBufferSize = 4096;
  static constexpr int kMaxKeepAliveRequests = 100;
  static constexpr size_t kMinThreads = 4;
  static constexpr size_t kMaxThreads = 32;

  const Port d_port;
  Port d_redirection_port;
  int server_fd{-1};
  int redirection_server_fd{-1};
  std::atomic<bool> d_running{false};
  std::unique_ptr<ThreadPool> d_thread_pool;
  bool https_enabled{false};
  bool http_redirection_enabled{false};
  std::string cert_path;
  std::string key_path;
  SSL_CTX* ssl_ctx{nullptr};

  template <typename Reader, typename Writer>
  void init_request_processor(int client_fd, Reader readFunc, Writer writeFunc,
                              bool isTLS = false, SSL* ssl = nullptr);
  bool init_ssl_context();
  void cleanup_ssl_context();
  void dispatch_client(int client_fd);
  void handle_client(SSL* ssl);
  void handle_client(int client_fd);
  void start_http_redirect(const Port& redirection_port);
};

template <typename Reader, typename Writer>
void Server::init_request_processor(int client_fd, Reader readFunc,
                                    Writer writeFunc, bool isTLS, SSL* ssl) {
  LOG_INFO("Client [" + std::to_string(client_fd) + "] connected" +
           (isTLS ? " via secure TLS" : ""));

  HttpParser parser;
  std::string recvBuffer;
  bool keepAlive = true;
  int requests_handled = 0;
  while (keepAlive && requests_handled < kMaxKeepAliveRequests) {
    char buffer[kRecvBufferSize];
    int bytes = readFunc(buffer, sizeof(buffer));
    if (bytes <= 0) {
      if (bytes == 0)
        LOG_INFO("Client [" + std::to_string(client_fd) +
                 "] closed connection");
      else if (errno == EAGAIN || errno == EWOULDBLOCK)
        LOG_INFO("Client [" + std::to_string(client_fd) +
                 "] idle timeout reached, closing");
      else
        LOG_ERROR("Fatal: Client [" + std::to_string(client_fd) +
                  "] recv error");
      break;
    }

    recvBuffer.append(buffer, bytes);
    std::string_view view(recvBuffer);
    ParseResult result = parser.parse(view);

    // Remove consumed bytes
    recvBuffer.erase(0, recvBuffer.size() - view.size());

    if (result == ParseResult::NEED_MORE_DATA) {
      continue;
    }

    if (result == ParseResult::PARSE_ERROR) {
      LOG_ERROR("Bad HTTP request from client [" + std::to_string(client_fd) +
                "]");
      HttpResponse resp = Responses::badRequest();
      auto payload = resp.serialize();
      writeFunc(payload.c_str(), payload.size());
      break;
    }

    // REQUEST_COMPLETE
    HttpRequest request = parser.takeRequest();

    LOG_INFO("Parsed request from client [" + std::to_string(client_fd) +
             "]: " + request.method + " " + request.path);

    HttpResponse response = Router::instance().route(request);
    keepAlive = requestWantsKeepAlive(request);

    std::string payload = response.serialize();
    bytes = writeFunc(payload.c_str(), payload.size());
    if (bytes <= 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        LOG_INFO("Client [" + std::to_string(client_fd) +
                "] send timeout reached, closing");
      }
    }

    requests_handled++;
  }

  if (isTLS && ssl) {
    SSL_shutdown(ssl);
    SSL_free(ssl);
  }

  close(client_fd);
  LOG_INFO("Client [" + std::to_string(client_fd) + "] disconnected" +
           (isTLS ? " (Secure TLS)" : ""));
}

}  // namespace HTTPServer

#endif