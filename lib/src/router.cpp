#include "httpserver/router.h"

#include <string>
#include <sstream>

#include "httpserver/http_object.h"
#include "httpserver/http_response_builder.h"
#include "httpserver/logger.h"

namespace HTTPServer {

Router& Router::instance() {
    static Router router;
    return router;
}

void Router::addRoute(const std::string& method, const std::string& path, RequestHandler handler) {
    if (path.find('{') != std::string::npos) {
        d_dynamicRoutes[method].push_back({path, handler});
    } else {
        d_routes[method][path] = handler;
    }
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

bool Router::matchDynamic(const std::string& pattern,
                          const std::string& path,
                          HttpRequest& req) const
{
    std::stringstream p(pattern), u(path);
    std::string segP, segU;

    while (std::getline(p, segP, '/') && std::getline(u, segU, '/')) {
        if (!segP.empty() && segP.front() == '{' && segP.back() == '}') {
            std::string key = segP.substr(1, segP.size() - 2);
            req.params[key] = segU;
            continue;
        }

        if (segP != segU) {
            req.params.clear();
            return false;
        }
    }

    // Ensure no extra segments exist in path
    if (std::getline(u, segU, '/')) {
        req.params.clear();
        return false;
    }

    // Ensure no pattern segments left unmatched
    if (std::getline(p, segP, '/')) {
        req.params.clear();
        return false;
    }

    return true;
}

HttpResponse Router::route(HttpRequest& request) const {
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

    // Try dynamic routes /{uuid}
    auto it = d_dynamicRoutes.find(request.method);
    if (it != d_dynamicRoutes.end()) {
        for (auto& dynamicRoute : it->second) {
            if (matchDynamic(dynamicRoute.d_pattern, request.path, request)) {
                return dynamicRoute.d_handler(request);
            }
        }
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