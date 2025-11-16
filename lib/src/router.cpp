#include "httpserver/router.h"

#include <string>

#include "httpserver/http_response.h"
#include "httpserver/http_response_builder.h"

namespace HTTPServer {

Router& Router::instance() {
    static Router instance;
    return instance;
}

void Router::addRoute(const std::string& method, const std::string& path, RequestHandler handler) {
    d_routes[method][path] = handler;
}

HttpResponse Router::route(const HttpRequest& request) const {
    auto methodIt = d_routes.find(request.method);
    if (methodIt != d_routes.end()) {
        const auto& pathMap = methodIt->second;
        auto pathIt = pathMap.find(request.path);
        if (pathIt != pathMap.end()) {
            return pathIt->second(request);
        }
    }
    return Responses::notFound(request);
}

} // namespace HTTPServer