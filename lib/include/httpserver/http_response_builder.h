#ifndef HTTP_RESPONSE_BUILDER_H
#define HTTP_RESPONSE_BUILDER_H

#include <string>

#include "http_response.h"

namespace HTTPServer {

namespace Responses {

HttpResponse ok(const HttpRequest&, const std::string&, const std::string& = "text/plain");
HttpResponse notFound(const HttpRequest&);
HttpResponse badRequest();
HttpResponse file(const HttpRequest&, const std::string&);

} // namespace Responses

} // namespace HTTPServer

#endif