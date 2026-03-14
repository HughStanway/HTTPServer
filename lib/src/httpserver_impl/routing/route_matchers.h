#ifndef ROUTE_MATCHERS_H
#define ROUTE_MATCHERS_H

#include <httpserver_impl/http/http_object.h>

#include <functional>
#include <string>

namespace HTTPServer {

using RequestHandler = std::function<HttpResponse(const HttpRequest&)>;

class IRouteMatcher {
 public:
  virtual ~IRouteMatcher() = default;

  // Evaluates if the path matches this route strategy.
  virtual bool match(const std::string& path, HttpRequest& req) const = 0;
  virtual RequestHandler getHandler() const = 0;
  virtual const std::string& getPattern() const = 0;
};

class ExactRouteMatcher : public IRouteMatcher {
 public:
  ExactRouteMatcher(std::string pattern, RequestHandler handler);
  bool match(const std::string& path, HttpRequest& req) const override;
  RequestHandler getHandler() const override;
  const std::string& getPattern() const override;

 private:
  std::string d_pattern;
  RequestHandler d_handler;
};

class DynamicRouteMatcher : public IRouteMatcher {
 public:
  DynamicRouteMatcher(std::string pattern, RequestHandler handler);
  bool match(const std::string& path, HttpRequest& req) const override;
  RequestHandler getHandler() const override;
  const std::string& getPattern() const override;

 private:
  std::string d_pattern;
  RequestHandler d_handler;
};

class WildcardRouteMatcher : public IRouteMatcher {
 public:
  WildcardRouteMatcher(std::string pattern, RequestHandler handler);
  bool match(const std::string& path, HttpRequest& req) const override;
  RequestHandler getHandler() const override;
  const std::string& getPattern() const override;

  // Used to determine the most specific wildcard match
  size_t getPrefixLength() const;

 private:
  std::string d_pattern;
  std::string d_prefix;
  RequestHandler d_handler;
};

}  // namespace HTTPServer

#endif
