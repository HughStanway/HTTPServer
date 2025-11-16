#ifndef HTTPPARSER_H
#define HTTPPARSER_H

#include "httpobject.h"

namespace HTTPServer {

enum class ParseError {
    NONE,
    EMPTY_REQUEST,
    INVALID_REQUEST_LINE,
    INVALID_METHOD,
    INVALID_VERSION,
    INVALID_HEADER_FORMAT,
    INVALID_HEADER_NAME,
};

class HttpParser {
  public:
    static ParseError parse(const std::string &, HttpRequest &);

  private:
    static void trim(std::string &);
    static bool isValidMethod(const std::string &);
    static bool isValidVersion(const std::string &);
    static bool isValidHeaderName(const std::string &);
};

} // namespace HTTPServer

#endif