#include <httpserver/http/http_object.h>
#include <httpserver/http/http_response_builder.h>
#include <httpserver/monitoring/logger.h>
#include <httpserver/routing/router.h>

#include <sstream>
#include <string>

namespace HTTPServer {

Router& Router::instance() {
  static Router router;
  return router;
}

void Router::addRoute(const std::string& method, const std::string& path,
                      RequestHandler handler) {
  auto& matchers = d_matchers[method];
  if (path.find('*') != std::string::npos) {
    for (auto& m : matchers.wildcard) {
      if (m->getPattern() == path) {
        m = std::make_unique<WildcardRouteMatcher>(path, handler);
        return;
      }
    }
    matchers.wildcard.push_back(
        std::make_unique<WildcardRouteMatcher>(path, handler));
  } else if (path.find('{') != std::string::npos) {
    for (auto& m : matchers.dynamic) {
      if (m->getPattern() == path) {
        m = std::make_unique<DynamicRouteMatcher>(path, handler);
        return;
      }
    }
    matchers.dynamic.push_back(
        std::make_unique<DynamicRouteMatcher>(path, handler));
  } else {
    for (auto& m : matchers.exact) {
      if (m->getPattern() == path) {
        m = std::make_unique<ExactRouteMatcher>(path, handler);
        return;
      }
    }
    matchers.exact.push_back(
        std::make_unique<ExactRouteMatcher>(path, handler));
  }
}

void Router::addStaticDirectoryRoute(const std::string& urlBase,
                                     const std::string& directory) {
  addRoute("GET", urlBase + "*", [directory, urlBase](const HttpRequest& req) {
    std::string relative = req.path.substr(urlBase.size());
    if (relative.empty() || relative == "/") relative = "/index.html";

    // Sanitize 'bad' input
    if (relative.find("..") != std::string::npos)
      return Responses::badRequest(StatusCode::Unauthorized);

    std::string fullPath = directory + relative;
    return Responses::file(req, fullPath);
  });
}

HttpResponse Router::route(HttpRequest& request) const {
  auto methodIt = d_matchers.find(request.method);
  if (methodIt == d_matchers.end()) {
    return Responses::notFound(request);
  }

  const auto& matchers = methodIt->second;

  // 1. Try exact matches first
  for (const auto& matcher : matchers.exact) {
    if (matcher->match(request.path, request)) {
      return matcher->getHandler()(request);
    }
  }

  // 2. Try dynamic routes next
  for (const auto& matcher : matchers.dynamic) {
    if (matcher->match(request.path, request)) {
      return matcher->getHandler()(request);
    }
  }

  // 3. Try wildcard routes (longest prefix wins)
  const IRouteMatcher* bestMatcher = nullptr;
  size_t bestPrefixLen = 0;

  for (const auto& matcher : matchers.wildcard) {
    if (matcher->match(request.path, request)) {
      auto wildcardMatcher =
          static_cast<const WildcardRouteMatcher*>(matcher.get());
      if (wildcardMatcher->getPrefixLength() >= bestPrefixLen) {
        bestPrefixLen = wildcardMatcher->getPrefixLength();
        bestMatcher = matcher.get();
      }
    }
  }

  if (bestMatcher) {
    return bestMatcher->getHandler()(request);
  }

  return Responses::notFound(request);
}

}  // namespace HTTPServer