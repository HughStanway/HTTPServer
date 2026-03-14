#include <httpserver/routing/route_matchers.h>

#include <sstream>

namespace HTTPServer {

ExactRouteMatcher::ExactRouteMatcher(std::string pattern,
                                     RequestHandler handler)
    : d_pattern(std::move(pattern)), d_handler(std::move(handler)) {}

bool ExactRouteMatcher::match(const std::string& path,
                              HttpRequest& /* req */) const {
  return d_pattern == path;
}

RequestHandler ExactRouteMatcher::getHandler() const { return d_handler; }

const std::string& ExactRouteMatcher::getPattern() const { return d_pattern; }

DynamicRouteMatcher::DynamicRouteMatcher(std::string pattern,
                                         RequestHandler handler)
    : d_pattern(std::move(pattern)), d_handler(std::move(handler)) {}

bool DynamicRouteMatcher::match(const std::string& path,
                                HttpRequest& req) const {
  std::stringstream p(d_pattern), u(path);
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

RequestHandler DynamicRouteMatcher::getHandler() const { return d_handler; }

const std::string& DynamicRouteMatcher::getPattern() const { return d_pattern; }

WildcardRouteMatcher::WildcardRouteMatcher(std::string pattern,
                                           RequestHandler handler)
    : d_pattern(std::move(pattern)), d_handler(std::move(handler)) {
  // Strip the trailing asterisk
  if (!d_pattern.empty() && d_pattern.back() == '*') {
    d_prefix = d_pattern.substr(0, d_pattern.size() - 1);
  } else {
    d_prefix = d_pattern;
  }
}

bool WildcardRouteMatcher::match(const std::string& path,
                                 HttpRequest& /* req */) const {
  return path.starts_with(d_prefix);
}

RequestHandler WildcardRouteMatcher::getHandler() const { return d_handler; }

const std::string& WildcardRouteMatcher::getPattern() const {
  return d_pattern;
}

size_t WildcardRouteMatcher::getPrefixLength() const { return d_prefix.size(); }

}  // namespace HTTPServer
