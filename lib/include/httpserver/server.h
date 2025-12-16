#ifndef SERVER_H
#define SERVER_H

#include <openssl/ssl.h>
#include <unistd.h>

#include <atomic>
#include <thread>
#include <vector>

#include "httpserver/http_object.h"
#include "httpserver/http_parser.h"
#include "httpserver/http_response_builder.h"
#include "httpserver/logger.h"
#include "httpserver/port.h"
#include "httpserver/router.h"
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
  static constexpr int kClientRecvTimeoutSec = 5;
  static constexpr int kDefaultHttpRedirectPort = 8080;
  static constexpr size_t kRecvBufferSize = 4096;
  static constexpr int kMaxKeepAliveRequests = 100;

  const Port d_port;
  Port d_redirection_port;
  int server_fd{-1};
  int redirection_server_fd{-1};
  std::atomic<bool> d_running{false};
  std::vector<std::thread> client_threads;
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

    std::string raw(buffer, bytes);
    HttpRequest request;
    ParseError err = HttpParser::parse(raw, request);
    HttpResponse response;
    if (err != ParseError::NONE) {
      LOG_ERROR("Bad HTTP request from client [" + std::to_string(client_fd) +
                "]: " + request.method + " " + request.path);
      response = Responses::badRequest();
      keepAlive = false;
    } else {
      LOG_INFO("Parsed request from client [" + std::to_string(client_fd) +
               "]: " + request.method + " " + request.path);
      response = Router::instance().route(request);
      keepAlive = requestWantsKeepAlive(request);
    }

    std::string payload = response.serialize();
    writeFunc(payload.c_str(), payload.size());

    requests_handled++;
    if (!keepAlive) break;
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