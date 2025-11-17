#include "httpserver/router.h"

#include <string>

#include "httpserver/http_response.h"
#include "httpserver/http_response_builder.h"
#include "httpserver/logger.h"

namespace HTTPServer {

Router& Router::instance() {
    static Router instance;
    return instance;
}

void Router::addRoute(const std::string& method, const std::string& path, RequestHandler handler) {
    d_routes[method][path] = handler;
}

void Router::addStaticDirectoryRoute(const std::string& urlBase, const std::string& directory) {
    addRoute("GET", urlBase + "*", [directory, urlBase](const HttpRequest& req) {
        std::string relative = req.path.substr(urlBase.size());
        if (relative.empty() || relative == "/") relative = "/index.html";

        // Sanitize 'bad' input
        if (relative.find("..") != std::string::npos)
            return Responses::badRequest();

        std::string fullPath = directory + relative;
        return Responses::file(req, fullPath);
    });
}

HttpResponse Router::route(const HttpRequest& request) const {
    auto methodIt = d_routes.find(request.method);
    if (methodIt == d_routes.end()) {
        return Responses::notFound(request);
    }

    const auto& pathMap = methodIt->second;

    // Try exact match first
    auto pathIt = pathMap.find(request.path);
    if (pathIt != pathMap.end()) {
        return pathIt->second(request);
    }

    // Try wildcard /* static-prefix routes
    const RequestHandler* bestHandler = nullptr;
    size_t bestPrefixLen = 0;

    for (const auto& [pattern, handler] : pathMap) {
        if (pattern.size() > 1 && pattern.ends_with("*")) {
            std::string prefix = pattern.substr(0, pattern.size() - 1);
            if (request.path.starts_with(prefix)) {
                if (prefix.size() > bestPrefixLen) {
                    bestPrefixLen = prefix.size();
                    bestHandler = &handler;
                }
            }
        }
    }

    if (bestHandler) {
        return (*bestHandler)(request);
    }
    return Responses::notFound(request);
}

} // namespace HTTPServer