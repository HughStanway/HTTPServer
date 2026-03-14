#ifndef CONNECTION_HANDLER_H
#define CONNECTION_HANDLER_H

#include <httpserver/core/connection_guard.h>
#include <httpserver/http/http_object.h>
#include <openssl/ssl.h>

#include <chrono>
#include <functional>
#include <string>
#include <string_view>

namespace HTTPServer {

class ConnectionHandler {
 public:
  using ReadFunc = std::function<int(char*, size_t)>;
  using WriteFunc = std::function<int(const char*, size_t)>;

  ConnectionHandler(int client_fd, const std::string& client_ip, bool isTLS,
                    SSL* ssl, ReadFunc readFunc, WriteFunc writeFunc,
                    bool isRedirectionServer = false);

  void process();

 private:
  int client_fd_;
  std::string client_ip_;
  bool isTLS_;
  SSL* ssl_;
  ReadFunc readFunc_;
  WriteFunc writeFunc_;
  bool isRedirectionServer_;

  bool is_tls_handshake_attempt(const std::string& data);
};

}  // namespace HTTPServer

#endif
