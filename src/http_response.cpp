#include "http_response.h"

#include <string>
#include <sstream>

#include "http_object.h"
#include "utils.h"

namespace HTTPServer {

HttpResponse::HttpResponse()
    : d_code(StatusCode::InternalServerError),
      d_version("HTTP/1.1") {}

HttpResponse& HttpResponse::setStatus(StatusCode code) {
    d_code = code;
    return *this;
}

HttpResponse& HttpResponse::addHeader(const std::string& key, const std::string& value) {
    d_headers[key] = value;
    return *this;
}

HttpResponse& HttpResponse::setBody(const std::string& body) {
    d_body = body;
    d_headers["Content-Length"] = std::to_string(body.size());
    return *this;
}

HttpResponse& HttpResponse::applyRequestDefaults(const HttpRequest& request) {
    if (!request.version.empty()) {
        d_version = request.version;
    }

    if (requestWantsKeepAlive(request)) {
        d_headers["Connection"] = "keep-alive";
    } else {
        d_headers["Connection"] = "close";
    }

    return *this;
}

std::string HttpResponse::serialize() const {
    std::ostringstream out;

    out << d_version << " " 
        << static_cast<int>(d_code) << " "
        << statusCodeToString(d_code) << "\r\n";

    for (const auto& [key, value] : d_headers) {
        out << key << ": " << value << "\r\n";
    }

    out << "\r\n"
        << d_body;

    return out.str();
}

} // namespace HTTPServer