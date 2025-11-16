#ifndef HTTPOBJECT_H
#define HTTPOBJECT_H

#include <string>
#include <unordered_map>

namespace HTTPServer {

struct HttpRequest {
    std::string method;
    std::string path;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
};

}  // namespace HTTPServer

#endif