#ifndef CONNECTION_HANDLER_H
#define CONNECTION_HANDLER_H

#include <openssl/ssl.h>

#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include "http_object.h"
#include "httpserver/connection_guard.h"

namespace HTTPServer {

class ConnectionHandler {
 public:
  using ReadFunc = std::function<int(char*, size_t)>;
  using WriteFunc = std::function<int(const char*, size_t)>;

  ConnectionHandler(int client_fd, const std::string& client_ip, bool isTLS,
                    SSL* ssl, ReadFunc readFunc, WriteFunc writeFunc,
                    std::mutex& ips_mtx,
                    std::unordered_map<std::string, ConnectedIp>& connected_ips,
                    bool isRedirectionServer = false);

  void process();

 private:
  int client_fd_;
  std::string client_ip_;
  bool isTLS_;
  SSL* ssl_;
  ReadFunc readFunc_;
  WriteFunc writeFunc_;
  std::mutex& ips_mtx_;
  std::unordered_map<std::string, ConnectedIp>& connected_ips_;
  bool isRedirectionServer_;

  bool is_tls_handshake_attempt(const std::string& data);
  void refill_tokens(ConnectedIp& client_ip);
  bool consume_token(ConnectedIp& client_ip);
  bool allow_request_from_ip(const std::string& ip);
};

}  // namespace HTTPServer

#endif
