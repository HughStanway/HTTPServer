#include "httpparser.h"

#include <sstream>
#include <string>
#include <unordered_map>

namespace HTTPServer {

ParseError HttpParser::parse(const std::string& raw, HttpRequest& request) {
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

    if (!line.empty() && line.back() == '\r') line.pop_back();

    std::istringstream request_line(line);
    if (!(request_line >> request.method >> request.path >> request.version)) {
        return ParseError::INVALID_REQUEST_LINE;
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
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (line.empty()) {
            break;  // end of headers
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

void HttpParser::trim(std::string& s) {
    while (!s.empty() && std::isspace((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && std::isspace((unsigned char)s.back())) s.pop_back();
}

bool HttpParser::isValidMethod(const std::string& m) {
    for (char c : m)
        if (!std::isupper((unsigned char)c)) return false;
    return !m.empty();
}

bool HttpParser::isValidVersion(const std::string& v) { return v.rfind("HTTP/", 0) == 0 && v.size() >= 6; }

bool HttpParser::isValidHeaderName(const std::string& name) {
    if (name.empty()) return false;
    for (char c : name) {
        if (!(std::isalnum((unsigned char)c) || c == '-' || c == '_')) return false;
    }
    return true;
}

}  // namespace HTTPServer