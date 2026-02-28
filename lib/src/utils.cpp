#include "httpserver/utils.h"

#include <algorithm>
#include <string>

#include "httpserver/http_object.h"

namespace HTTPServer {

std::string statusCodeToString(StatusCode code) {
  switch (code) {
    case StatusCode::OK:
      return "OK";
    case StatusCode::MovedPermanently:
      return "Moved Permanently";
    case StatusCode::BadRequest:
      return "Bad Request";
    case StatusCode::Unauthorized:
      return "Unauthorized";
    case StatusCode::Forbidden:
      return "Forbidden";
    case StatusCode::NotFound:
      return "Not Found";
    case StatusCode::RequestTimeout:
      return "Request Timeout";
    case StatusCode::PayloadTooLarge:
      return "Payload Too Large";
    case StatusCode::UnsupportedMediaType:
      return "Unsupported Media Type";
    case StatusCode::RequestHeaderFieldsTooLarge:
      return "Request Header Fields Too Large";
    case StatusCode::InternalServerError:
      return "Internal Server Error";
    case StatusCode::NotImplemented:
      return "Not Implemented";
    case StatusCode::BadGateway:
      return "Bad Gateway";
    case StatusCode::ServiceUnavailable:
      return "Service Unavailable";
    default:
      return "Unknown";
  }
}

std::string statusCodeToNumericString(StatusCode code) {
  return std::to_string(static_cast<int>(code));
}

StatusCode parseErrorToStatusCode(ParseError err) {
  switch (err) {
    case ParseError::BODY_TOO_LARGE:
      return StatusCode::PayloadTooLarge;  // 413

    case ParseError::TOTAL_HEADER_SIZE_TOO_LARGE:
    case ParseError::HEADER_TOO_LARGE:
      return StatusCode::RequestHeaderFieldsTooLarge;  // 431

    case ParseError::INVALID_REQUEST_LINE_FORMAT:
    case ParseError::REQUEST_METHOD_EMPTY:
    case ParseError::REQUEST_METHOD_LOWERCASE:
    case ParseError::UNSUPPORTED_METHOD:
    case ParseError::UNSUPPORTED_VERSION:
    case ParseError::INVALID_HEADER_MISSING_COLON:
    case ParseError::INVALID_HEADER_UNSUPPORTED_CHARACTER:
    case ParseError::MISSING_HOST_HEADER:
    case ParseError::CONTENT_LENGTH_EMPTY:
    case ParseError::CONTENT_LENGTH_NOT_NUMERIC:
    case ParseError::INVALID_CHUNKED_ENCODING:
    case ParseError::TOO_MANY_HEADERS:
      return StatusCode::BadRequest;  // 400

    default:
      return StatusCode::BadRequest;
  }
}

std::string generateErrorPage(StatusCode code) {
  int status = static_cast<int>(code);
  std::string reason = statusCodeToString(code);

  std::string body;
  body.reserve(256);

  body += "<!DOCTYPE html>";
  body += "<html lang=\"en\">\n";
  body += "<head><title>";
  body += std::to_string(status);
  body += " ";
  body += reason;
  body += "</title></head>\n";
  body += "<body>\n";
  body += "<center><h1>";
  body += std::to_string(status);
  body += " ";
  body += reason;
  body += "</h1></center>\n";
  body += "<hr><center>HTTPServer</center>\n";
  body += "</body>\n";
  body += "</html>\n";

  return body;
}

bool requestWantsKeepAlive(const HttpRequest& req) {
  auto it = req.headers.find("Connection");
  if (it != req.headers.end()) {
    std::string value = it->second.back();
    std::transform(value.begin(), value.end(), value.begin(), ::tolower);
    return value == "keep-alive";
  }

  if (req.version == "HTTP/1.1") return true;

  return false;
}

std::string Mime::fromExtension(const std::string& path) {
  auto pos = path.find_last_of('.');
  if (pos == std::string::npos) return "application/octet-stream";

  std::string ext = path.substr(pos + 1);

  if (ext == "html") return "text/html";
  if (ext == "css") return "text/css";
  if (ext == "js") return "application/javascript";
  if (ext == "png") return "image/png";
  if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
  if (ext == "svg") return "image/svg+xml";
  if (ext == "json") return "application/json";

  return "application/octet-stream";
}

std::optional<std::string_view> get_last_header_value(const HttpRequest& req,
                                                      std::string_view name) {
  auto it = req.headers.find(std::string(name));
  if (it == req.headers.end() || it->second.empty()) return std::nullopt;

  return it->second.back();
}

}  // namespace HTTPServer