#include "httpserver/http_parser.h"

#include <sstream>
#include <string>
#include <unordered_map>

namespace HTTPServer {

ParseError HttpParser::parse(const std::string &raw, HttpRequest &request) {
    if (raw.empty()) {
        return ParseError::EMPTY_REQUEST;
    }

    std::istringstream stream(raw);
    std::string line;

    // -----------------------------
    // Parse request line
    // -----------------------------
    if (!std::getline(stream, line)) {
        return ParseError::INVALID_REQUEST_LINE;
    }

    if (!line.empty() && line.back() == '\r')
        line.pop_back();

    std::istringstream request_line(line);
    if (!(request_line >> request.method >> request.path >> request.version)) {
        return ParseError::INVALID_REQUEST_LINE;
    }

    // -------------------------------------
    // Extract and parse query string
    // -------------------------------------
    size_t qmark = request.path.find('?');
    if (qmark != std::string::npos) {
        std::string qs = request.path.substr(qmark + 1);
        request.path = request.path.substr(0, qmark);

        parseQueryParams(qs, request.params);
    }

    // -----------------------------
    // Validate method
    // -----------------------------
    if (!isValidMethod(request.method)) {
        return ParseError::INVALID_METHOD;
    }

    // -----------------------------
    // Validate HTTP version
    // -----------------------------
    if (!isValidVersion(request.version)) {
        return ParseError::INVALID_VERSION;
    }

    // -----------------------------
    // Parse headers
    // -----------------------------
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        if (line.empty()) {
            break; // end of headers
        }

        auto colon = line.find(':');
        if (colon == std::string::npos) {
            return ParseError::INVALID_HEADER_FORMAT;
        }

        std::string name = line.substr(0, colon);
        std::string value = line.substr(colon + 1);

        trim(value);

        if (!isValidHeaderName(name)) {
            return ParseError::INVALID_HEADER_NAME;
        }

        request.headers[name] = value;
    }

    // -----------------------------
    // Parse body
    // -----------------------------
    std::string body;
    while (std::getline(stream, line)) {
        body.append(line);
        body.push_back('\n');
    }
    request.body = body;

    return ParseError::NONE;
}

void HttpParser::trim(std::string &s) {
    while (!s.empty() && std::isspace((unsigned char)s.front()))
        s.erase(s.begin());
    while (!s.empty() && std::isspace((unsigned char)s.back()))
        s.pop_back();
}

bool HttpParser::isValidMethod(const std::string &m) {
    for (char c : m)
        if (!std::isupper((unsigned char)c))
            return false;
    return !m.empty();
}

bool HttpParser::isValidVersion(const std::string &v) { return v.rfind("HTTP/", 0) == 0 && v.size() >= 6; }

bool HttpParser::isValidHeaderName(const std::string &name) {
    if (name.empty())
        return false;
    for (char c : name) {
        if (!(std::isalnum((unsigned char)c) || c == '-' || c == '_'))
            return false;
    }
    return true;
}

std::string HttpParser::urlDecode(const std::string& s) {
    std::string out;
    out.reserve(s.size());

    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 2 < s.size()) {
            char hex[3] = { s[i+1], s[i+2], 0 };
            out.push_back(static_cast<char>(std::strtol(hex, nullptr, 16)));
            i += 2;
        } else if (s[i] == '+') {
            out.push_back(' ');
        } else {
            out.push_back(s[i]);
        }
    }
    return out;
}

void HttpParser::parseQueryParams(const std::string& qs,
                             std::unordered_map<std::string, std::string>& map)
{
    size_t start = 0;

    while (start < qs.size()) {
        size_t amp = qs.find('&', start);
        if (amp == std::string::npos)
            amp = qs.size();

        std::string pair = qs.substr(start, amp - start);

        size_t eq = pair.find('=');
        if (eq != std::string::npos) {
            std::string key = urlDecode(pair.substr(0, eq));
            std::string val = urlDecode(pair.substr(eq + 1));
            map[ key ] = val;
        } else {
            // key with no value
            map[ urlDecode(pair) ] = "";
        }

        start = amp + 1;
    }
}

} // namespace HTTPServer