#include "httpserver/utils.h"

#include <string>
#include <algorithm>

#include "httpserver/http_object.h"

namespace HTTPServer {

std::string statusCodeToString(StatusCode code) {
    switch (code) {
        case StatusCode::OK:
            return "OK";
        case StatusCode::BadRequest:
            return "Bad Request";
        case StatusCode::NotFound:
            return "Not Found";
        case StatusCode::InternalServerError:
            return "Internal Server Error";
        default:
            return "Unknown";
    }
}

bool requestWantsKeepAlive(const HttpRequest& req) {
    auto it = req.headers.find("Connection");
    if (it != req.headers.end()) {
        std::string value = it->second;
        std::transform(value.begin(), value.end(), value.begin(), ::tolower);
        return value == "keep-alive";
    }

    if (req.version == "HTTP/1.1")
        return true;

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

} // namespace HTTPServer