#include <httpserver/core/connection_handler.h>
#include <httpserver/events/event_dispatcher.h>
#include <httpserver/events/events.h>
#include <httpserver/http/http_parser.h>
#include <httpserver/http/http_response_builder.h>
#include <httpserver/monitoring/log_event.h>
#include <httpserver/monitoring/logger.h>
#include <httpserver/routing/router.h>
#include <httpserver/utils/config.h>
#include <httpserver/utils/utils.h>
#include <unistd.h>

namespace HTTPServer {

enum class RequestState { Idle, Receiving };

ConnectionHandler::ConnectionHandler(
    int client_fd, const std::string& client_ip, bool isTLS, SSL* ssl,
    ReadFunc readFunc, WriteFunc writeFunc,
    bool isRedirectionServer)
    : client_fd_(client_fd),
      client_ip_(client_ip),
      isTLS_(isTLS),
      ssl_(ssl),
      readFunc_(std::move(readFunc)),
      writeFunc_(std::move(writeFunc)),
      isRedirectionServer_(isRedirectionServer) {}

bool ConnectionHandler::is_tls_handshake_attempt(const std::string& data) {
  if (data.size() < 3) {
    return false;
  }

  unsigned char byte0 = static_cast<unsigned char>(data[0]);
  unsigned char byte1 = static_cast<unsigned char>(data[1]);
  unsigned char byte2 = static_cast<unsigned char>(data[2]);

  if (byte0 != 0x16) {
    return false;
  }

  if (byte1 == 0x03 && byte2 >= 0x01 && byte2 <= 0x03) {
    return true;
  }

  return false;
}

void ConnectionHandler::process() {
  LOG_EVENT(LogLevel::INFO, LogEvent("connected")
                                .add("client_fd", client_fd_)
                                .add("ip", client_ip_)
                                .add("tls", isTLS_));

  HttpParser parser;
  std::string recvBuffer;
  bool keepAlive = true;
  int requests_handled = 0;
  int total_request_bytes_received = 0;
  RequestState request_state = RequestState::Idle;
  std::chrono::steady_clock::time_point request_start;

  while (keepAlive) {
    if (request_state == RequestState::Idle) {
      request_state = RequestState::Receiving;
      request_start = std::chrono::steady_clock::now();
      total_request_bytes_received = 0;
    }

    char buffer[Config::get().kRecvBufferSize];
    int bytes = readFunc_(buffer, sizeof(buffer));
    if (bytes <= 0) {
      if (bytes == 0)
        LOG_EVENT(LogLevel::INFO, LogEvent("client_closed_connection")
                                      .add("client_fd", client_fd_)
                                      .add("ip", client_ip_)
                                      .add("tls", isTLS_));
      else if (errno == EAGAIN || errno == EWOULDBLOCK)
        LOG_EVENT(LogLevel::INFO, LogEvent("client_idle_timeout")
                                      .add("client_fd", client_fd_)
                                      .add("ip", client_ip_)
                                      .add("tls", isTLS_));
      else
        LOG_EVENT(LogLevel::ERROR, LogEvent("recv_error")
                                       .add("client_fd", client_fd_)
                                       .add("ip", client_ip_)
                                       .add("tls", isTLS_));
      break;
    } else {
      recvBuffer.append(buffer, bytes);
      total_request_bytes_received += bytes;
    }

    if (request_state == RequestState::Receiving) {
      auto now = std::chrono::steady_clock::now();
      if (now - request_start >
          std::chrono::seconds(Config::get().kMaxRequestDurationSec)) {
        LOG_EVENT(LogLevel::ERROR, LogEvent("request_timeout")
                                       .add("client_fd", client_fd_)
                                       .add("ip", client_ip_)
                                       .add("tls", isTLS_));
        HttpResponse resp = Responses::badRequest(StatusCode::RequestTimeout);
        auto payload = resp.serialize();
        bytes = writeFunc_(payload.c_str(), payload.size());
        EventDispatcher::instance().dispatch(RequestProcessedEvent(
            resp, request_start, total_request_bytes_received, bytes));
        break;
      }
    }

    if (!isTLS_ && is_tls_handshake_attempt(recvBuffer)) {
      LOG_EVENT(LogLevel::ERROR,
                LogEvent("client_attempted_tls_handshake_on_non_https_port")
                    .add("client_fd", client_fd_)
                    .add("ip", client_ip_)
                    .add("tls", isTLS_));
      break;
    }

    std::string_view view(recvBuffer);
    ParseResult result = parser.parse(view);

    recvBuffer.erase(0, recvBuffer.size() - view.size());

    if (result == ParseResult::NEED_MORE_DATA) {
      continue;
    }

    if (result == ParseResult::PARSE_ERROR) {
      ParseError err = parser.error();
      StatusCode status = parseErrorToStatusCode(err);
      LOG_EVENT(LogLevel::ERROR,
                LogEvent("bad_request")
                    .add("client_fd", client_fd_)
                    .add("ip", client_ip_)
                    .add("tls", isTLS_)
                    .add("parse_error", static_cast<int>(err)));
      HttpResponse resp = Responses::badRequest(status);
      auto payload = resp.serialize();
      bytes = writeFunc_(payload.c_str(), payload.size());

      EventDispatcher::instance().dispatch(RequestProcessedEvent(
          resp, request_start, total_request_bytes_received, bytes));
      break;
    }

    HttpRequest request = parser.takeRequest();
    LOG_EVENT(LogLevel::INFO, LogEvent("http_request")
                                  .add("client_fd", client_fd_)
                                  .add("ip", client_ip_)
                                  .add("tls", isTLS_)
                                  .add("method", request.method)
                                  .add("path", request.path));

    if (requests_handled >= Config::get().kMaxKeepAliveRequests) {
      LOG_EVENT(LogLevel::WARN, LogEvent("max_keep_alive_requests_exceeded")
                                    .add("client_fd", client_fd_)
                                    .add("ip", client_ip_)
                                    .add("tls", isTLS_)
                                    .add("max_keep_alive_requests",
                                         Config::get().kMaxKeepAliveRequests));
      HttpResponse resp = Responses::badRequest(StatusCode::TooManyRequests);
      auto payload = resp.serialize();
      bytes = writeFunc_(payload.c_str(), payload.size());

      EventDispatcher::instance().dispatch(RequestProcessedEvent(
          resp, request_start, total_request_bytes_received, bytes));
      break;
    }

    if (!ConnectionManager::instance().allowRequest(client_ip_)) {
      HttpResponse resp = Responses::badRequest(StatusCode::TooManyRequests);
      auto payload = resp.serialize();
      bytes = writeFunc_(payload.c_str(), payload.size());

      EventDispatcher::instance().dispatch(RequestProcessedEvent(
          resp, request_start, total_request_bytes_received, bytes));
      break;
    }

    HttpResponse response;
    if (isRedirectionServer_) {
      response = Responses::redirection(request, Config::get().kPort);
    } else {
      response = Router::instance().route(request);
    }

    if (isRedirectionServer_) {
      keepAlive = false;
    } else {
      keepAlive = requestWantsKeepAlive(request);
      if (auto it = response.headers.find("Connection");
          it != response.headers.end()) {
        if (it->second == "close") {
          keepAlive = false;
        }
      }
    }

    std::string payload = response.serialize();
    bytes = writeFunc_(payload.c_str(), payload.size());
    if (bytes <= 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        LOG_EVENT(LogLevel::ERROR, LogEvent("client_send_timeout")
                                       .add("client_fd", client_fd_)
                                       .add("ip", client_ip_)
                                       .add("tls", isTLS_));
      }
    }

    if (request_state == RequestState::Receiving) {
      EventDispatcher::instance().dispatch(RequestProcessedEvent(
          response, request_start, total_request_bytes_received, bytes));
      request_state = RequestState::Idle;
    }

    parser.reset();
    requests_handled++;
  }

  if (isTLS_ && ssl_) {
    SSL_shutdown(ssl_);
    SSL_free(ssl_);
  }

  close(client_fd_);
  if (isRedirectionServer_) {
    LOG_EVENT(LogLevel::INFO, LogEvent("client_disconnected")
                                  .add("client_fd", client_fd_)
                                  .add("ip", client_ip_)
                                  .add("redirection_server", true)
                                  .add("client_redirected", true));
  } else {
    LOG_EVENT(LogLevel::INFO, LogEvent("client_disconnected")
                                  .add("client_fd", client_fd_)
                                  .add("ip", client_ip_)
                                  .add("tls", isTLS_));
  }
}

}  // namespace HTTPServer
