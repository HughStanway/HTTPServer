#ifndef HTTP_OBJECT_H
#define HTTP_OBJECT_H

#include <string>
#include <unordered_map>

namespace HTTPServer {

enum class StatusCode {
    OK = 200,
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
};

} // namespace HTTPServer

#endif