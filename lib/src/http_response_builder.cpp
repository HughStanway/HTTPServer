#include "httpserver/http_response_builder.h"

#include <string>

#include "httpserver/http_response.h"

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

} // namespace Responses

} // namespace HTTPServer