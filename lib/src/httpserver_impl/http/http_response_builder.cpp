#include <httpserver_impl/http/http_object.h>
#include <httpserver_impl/http/http_response_builder.h>
#include <httpserver_impl/monitoring/logger.h>
#include <httpserver_impl/utils/port.h>
#include <httpserver_impl/utils/utils.h>

#include <fstream>
#include <sstream>
#include <string>

namespace HTTPServer {

namespace Responses {

HttpResponse ok(const HttpRequest& req, const std::string& body,
                const std::string& type) {
  HttpResponse res;
  res.setStatus(StatusCode::OK)
      .applyRequestDefaults(req)
      .addHeader("Content-Type", type)
      .setBody(body);
  return res;
}

HttpResponse notFound(const HttpRequest& req) {
  std::string body = generateErrorPage(StatusCode::NotFound);
  HttpResponse res;
  return res.setStatus(StatusCode::NotFound)
      .addHeader("Content-Type", "text/html")
      .addHeader("Connection", "close")
      .setBody(body);
}

HttpResponse badRequest(StatusCode code) {
  std::string body = generateErrorPage(code);
  HttpResponse res;
  return res.setStatus(code)
      .addHeader("Content-Type", "text/html")
      .addHeader("Connection", "close")
      .setBody(body);
}

HttpResponse redirection(const HttpRequest& req, const Port& port) {
  auto host_header = get_last_header_value(req, "Host");
  std::string host = host_header ? std::string(*host_header) : "localhost";

  auto colonPos = host.find(':');
  if (colonPos != std::string::npos) {
    host = host.substr(0, colonPos);
  }

  // Only include port if HTTPS is not on 443
  std::string portStr = (port.value() == 443) ? "" : ":" + port.toString();
  HttpResponse res;
  return res.setStatus(StatusCode::MovedPermanently)
      .addHeader("Location", "https://" + host + portStr + req.path)
      .setBody("301 Moved Permanently");
}

HttpResponse file(const HttpRequest& req, const std::string& filepath) {
  std::ifstream file(filepath, std::ios::binary);

  if (!file) {
    return Responses::notFound(req);
  }

  std::ostringstream buffer;
  buffer << file.rdbuf();
  std::string content = buffer.str();

  HttpResponse res;
  return res.setStatus(StatusCode::OK)
      .addHeader("Content-Length", std::to_string(content.size()))
      .addHeader("Content-Type", Mime::fromExtension(filepath))
      .setBody(content);
}

}  // namespace Responses

}  // namespace HTTPServer