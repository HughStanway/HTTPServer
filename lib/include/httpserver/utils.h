#ifndef UTILS_H
#define UTILS_H

#include "http_parser.h"
#include "http_object.h"

#include <optional>
#include <string>
#include <string_view>

namespace HTTPServer {

std::string statusCodeToString(StatusCode);
std::string statusCodeToNumericString(StatusCode);
StatusCode parseErrorToStatusCode(ParseError);
std::string generateErrorPage(StatusCode);
bool requestWantsKeepAlive(const HttpRequest&);

namespace Mime {

std::string fromExtension(const std::string&);

} // namespace Mime

std::optional<std::string_view> get_last_header_value(const HttpRequest& req, std::string_view name);

} // namespace HTTPServer

#endif