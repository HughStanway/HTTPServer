#include "httpserver/http_object.h"

#include <string>
#include <sstream>

#include "httpserver/utils.h"

namespace HTTPServer {

HttpResponse& HttpResponse::setStatus(StatusCode newCode) {
    code = newCode;
    return *this;
}

HttpResponse& HttpResponse::addHeader(const std::string& key, const std::string& value) {
    headers[key] = value;
    return *this;
}

HttpResponse& HttpResponse::setBody(const std::string& newBody) {
    body = newBody;
    headers["Content-Length"] = std::to_string(body.size());
    return *this;
}

HttpResponse& HttpResponse::applyRequestDefaults(const HttpRequest& request) {
    if (!request.version.empty()) {
        version = request.version;
    }

    if (requestWantsKeepAlive(request)) {
        headers["Connection"] = "keep-alive";
    } else {
        headers["Connection"] = "close";
    }

    return *this;
}

std::string HttpResponse::serialize() const {
    std::ostringstream out;

    out << version << " " 
        << static_cast<int>(code) << " "
        << statusCodeToString(code) << "\r\n";

    for (const auto& [key, value] : headers) {
        out << key << ": " << value << "\r\n";
    }

    out << "\r\n"
        << body;

    return out.str();
}    

} // namespace HTTPServer