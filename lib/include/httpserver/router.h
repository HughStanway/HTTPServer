#ifndef ROUTER_H
#define ROUTER_H

#include <functional>
#include <unordered_map>
#include <string>

#include "http_response.h"

namespace HTTPServer {

using RequestHandler = std::function<HttpResponse(const HttpRequest&)>;

class Router {
    public:
        static Router& instance();
        Router(const Router&) = delete;
        Router& operator=(const Router&) = delete;
        Router(Router&&) = delete;
        Router& operator=(Router&&) = delete;

        void addRoute(const std::string&, const std::string&, RequestHandler);
        void addStaticDirectoryRoute(const std::string&, const std::string&);
        HttpResponse route(const HttpRequest&) const;

    private:
        Router() = default;
        std::unordered_map<std::string, std::unordered_map<std::string, RequestHandler>> d_routes;
};

} // namespace HTTPServer

#endif