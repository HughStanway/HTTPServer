#ifndef HTTP_RESPONSE_BUILDER_H
#define HTTP_RESPONSE_BUILDER_H

#include <httpserver/http/http_object.h>
#include <httpserver/utils/port.h>

#include <string>

namespace HTTPServer {

namespace Responses {

HttpResponse ok(const HttpRequest&, const std::string&,
                const std::string& = "text/plain");
HttpResponse notFound(const HttpRequest&);
HttpResponse badRequest(StatusCode);
HttpResponse redirection(const HttpRequest&, const Port&);
HttpResponse file(const HttpRequest&, const std::string&);

}  // namespace Responses

}  // namespace HTTPServer

#endif