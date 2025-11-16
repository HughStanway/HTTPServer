#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <string>
#include <unordered_map>

#include "http_object.h"

namespace HTTPServer {

class HttpResponse {
    public:
        HttpResponse();

        HttpResponse& setStatus(StatusCode);
        HttpResponse& addHeader(const std::string&, const std::string&);
        HttpResponse& setBody(const std::string&);
        HttpResponse& applyRequestDefaults(const HttpRequest&);

        std::string serialize() const;

    private:
        StatusCode d_code;
        std::string d_version;
        std::unordered_map<std::string, std::string> d_headers;
        std::string d_body;
};

} // namespace HTTPServer

#endif