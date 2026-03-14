#ifndef ROUTER_H
#define ROUTER_H

#include <httpserver/http/http_object.h>
#include <httpserver/routing/route_matchers.h>

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace HTTPServer {

class Router {
 public:
  static Router& instance();

  void addRoute(const std::string&, const std::string&, RequestHandler);
  void addStaticDirectoryRoute(const std::string&, const std::string&);
  HttpResponse route(HttpRequest&) const;

 private:
  Router() = default;
  ~Router() = default;

  Router(const Router&) = delete;
  Router& operator=(const Router&) = delete;
  Router(Router&&) = delete;
  Router& operator=(Router&&) = delete;

  struct MethodMatchers {
    std::vector<std::unique_ptr<IRouteMatcher>> exact;
    std::vector<std::unique_ptr<IRouteMatcher>> dynamic;
    std::vector<std::unique_ptr<IRouteMatcher>> wildcard;
  };

  std::unordered_map<std::string, MethodMatchers> d_matchers;
};

}  // namespace HTTPServer

#endif