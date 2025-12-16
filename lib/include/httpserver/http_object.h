#ifndef HTTP_OBJECT_H
#define HTTP_OBJECT_H

#include <string>
#include <unordered_map>

namespace HTTPServer {

enum class StatusCode {
    OK = 200,
    MovedPermanently = 301,
    BadRequest = 400,
    NotFound = 404,
    InternalServerError = 500,
};

struct HttpRequest {
    std::string method;
    std::string path;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    std::unordered_map<std::string, std::string> params;
};

struct HttpResponse {
    StatusCode code = StatusCode::InternalServerError;
    std::string version = "HTTP/1.1";
    std::unordered_map<std::string, std::string> headers;
    std::string body;

    HttpResponse& setStatus(StatusCode);
    HttpResponse& addHeader(const std::string&, const std::string&);
    HttpResponse& setBody(const std::string&);
    HttpResponse& applyRequestDefaults(const HttpRequest&);

    std::string serialize() const;
};

} // namespace HTTPServer

#endif