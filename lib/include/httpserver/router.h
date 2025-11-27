#ifndef ROUTER_H
#define ROUTER_H

#include <functional>
#include <unordered_map>
#include <string>

#include "http_object.h"

namespace HTTPServer {

using RequestHandler = std::function<HttpResponse(const HttpRequest&)>;

struct DynamicRoute {
    std::string d_pattern;
    RequestHandler d_handler;
};

class Router {
    public:
        static Router& instance();
        Router(const Router&) = delete;
        Router& operator=(const Router&) = delete;
        Router(Router&&) = delete;
        Router& operator=(Router&&) = delete;

        void addRoute(const std::string&, const std::string&, RequestHandler);
        void addStaticDirectoryRoute(const std::string&, const std::string&);
        HttpResponse route(HttpRequest&) const;

    private:
        Router() = default;
        bool matchDynamic(const std::string&, const std::string&, HttpRequest&) const;
        std::unordered_map<std::string, std::unordered_map<std::string, RequestHandler>> d_routes;
        std::unordered_map<std::string, std::vector<DynamicRoute>> d_dynamicRoutes;
};

} // namespace HTTPServer

#endif