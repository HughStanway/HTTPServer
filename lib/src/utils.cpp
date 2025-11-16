#include "http_object.h"

#include <string>
#include <algorithm>

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

} // namespace HTTPServer