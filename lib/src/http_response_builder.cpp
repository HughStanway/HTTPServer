#include "httpserver/http_response_builder.h"

#include <string>
#include <fstream>
#include <sstream>
#include <string>

#include "httpserver/http_response.h"
#include "httpserver/utils.h"
#include "httpserver/logger.h"

namespace HTTPServer {

namespace Responses {

HttpResponse ok(const HttpRequest& req, const std::string& body, const std::string& type) {
    HttpResponse res;
    res.setStatus(StatusCode::OK)
        .applyRequestDefaults(req)
        .addHeader("Content-Type", type)
        .setBody(body);
    return res;
}

HttpResponse notFound(const HttpRequest& req) {
    HttpResponse res;
    return res.setStatus(StatusCode::NotFound)
              .addHeader("Content-Type", "text/plain")
              .addHeader("Connection", "close")
              .setBody("404 Not Found: " + req.path);
}

HttpResponse badRequest() {
    HttpResponse res;
    return res.setStatus(StatusCode::BadRequest)
              .addHeader("Content-Type", "text/plain")
              .addHeader("Connection", "close")
              .setBody("400 Bad Request");
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

} // namespace Responses

} // namespace HTTPServer