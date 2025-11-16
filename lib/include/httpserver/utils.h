#ifndef UTILS_H
#define UTILS_H

#include "http_object.h"

#include <string>

namespace HTTPServer {

std::string statusCodeToString(StatusCode);
bool requestWantsKeepAlive(const HttpRequest&);

namespace Mime {

std::string fromExtension(const std::string&);

} // namespace Mime

} // namespace HTTPServer

#endif