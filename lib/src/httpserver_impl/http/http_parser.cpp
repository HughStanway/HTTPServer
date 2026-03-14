#include <httpserver_impl/http/http_parser.h>

#include <unordered_set>

namespace {

static bool iequals(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(a[i])) !=
        std::tolower(static_cast<unsigned char>(b[i]))) {
      return false;
    }
  }
  return true;
}

}  // namespace

namespace HTTPServer {

HttpParser::HttpParser() { reset(); }

ParseResult HttpParser::parse(std::string_view& buffer) {
  while (true) {
    switch (d_state) {
      case ParseState::REQUEST_LINE:
        if (!parseRequestLine(buffer)) {
          if (d_state == ParseState::ERROR) {
            return ParseResult::PARSE_ERROR;
          }
          return ParseResult::NEED_MORE_DATA;
        }
        d_state = ParseState::HEADERS;
        break;

      case ParseState::HEADERS:
        if (!parseHeaders(buffer)) {
          if (d_state == ParseState::ERROR) {
            return ParseResult::PARSE_ERROR;
          }
          return ParseResult::NEED_MORE_DATA;
        }
        if (!determineBodyFraming()) {
          return ParseResult::PARSE_ERROR;
        }
        d_state = d_bodyType == BodyType::NONE ? ParseState::COMPLETE
                                               : ParseState::BODY;
        break;

      case ParseState::BODY:
        if (!parseBody(buffer)) {
          if (d_state == ParseState::ERROR) {
            return ParseResult::PARSE_ERROR;
          }
          return ParseResult::NEED_MORE_DATA;
        }
        d_state = ParseState::COMPLETE;
        break;

      case ParseState::COMPLETE:
        return ParseResult::REQUEST_COMPLETE;

      case ParseState::ERROR:
        return ParseResult::PARSE_ERROR;

      default:
        return ParseResult::PARSE_ERROR;
    }
  }
}

bool HttpParser::hasCompleteRequest() const {
  return d_state == ParseState::COMPLETE;
}

HttpRequest HttpParser::takeRequest() {
  d_state = ParseState::REQUEST_LINE;
  return std::move(d_request);
}

bool HttpParser::hasError() const { return d_state == ParseState::ERROR; }

ParseError HttpParser::error() const { return d_error; }

void HttpParser::reset() {
  d_state = ParseState::REQUEST_LINE;
  d_bodyType = BodyType::NONE;
  d_chunkState = ChunkState::SIZE;
  d_error = ParseError::NONE;
  d_request = HttpRequest{};
  d_lineBuffer.clear();
  d_bodyBytesRemaining = 0;
  d_currentChunkSize = 0;
  d_currentChunkRead = 0;
  d_currentHeaderCount = 0;
  d_currentTotalHeaderSize = 0;
}

bool HttpParser::parseRequestLine(std::string_view& buffer) {
  if (!consumeLine(buffer, d_lineBuffer)) return false;

  auto& line = d_lineBuffer;
  auto p1 = line.find(' ');
  if (p1 == std::string::npos) {
    d_error = ParseError::INVALID_REQUEST_LINE_FORMAT;
    d_state = ParseState::ERROR;
    return false;
  }

  auto p2 = line.find(' ', p1 + 1);
  if (p2 == std::string::npos) {
    d_error = ParseError::INVALID_REQUEST_LINE_FORMAT;
    d_state = ParseState::ERROR;
    return false;
  }

  if (p2 == p1 + 1) {  // Two spaces in a row
    d_error = ParseError::INVALID_REQUEST_LINE_FORMAT;
    d_state = ParseState::ERROR;
    return false;
  }

  // Check for third whitespace character
  if (line.find(' ', p2 + 1) != std::string::npos) {
    d_error = ParseError::INVALID_REQUEST_LINE_FORMAT;
    d_state = ParseState::ERROR;
    return false;
  }

  d_request.method = line.substr(0, p1);
  d_request.path = line.substr(p1 + 1, p2 - p1 - 1);
  d_request.version = line.substr(p2 + 1);

  if (d_request.path.empty()) {
    d_error = ParseError::INVALID_REQUEST_LINE_FORMAT;
    d_state = ParseState::ERROR;
    return false;
  }

  size_t qmark = d_request.path.find('?');
  if (qmark != std::string::npos) {
    std::string qs = d_request.path.substr(qmark + 1);
    d_request.path = d_request.path.substr(0, qmark);

    parseQueryParams(qs, d_request.params);
  }

  if (!validateRequestLine()) {
    d_state = ParseState::ERROR;
    return false;
  }

  d_state = ParseState::HEADERS;
  return true;
}

bool HttpParser::validateRequestLine() {
  if (d_request.method.empty()) {
    d_error = ParseError::REQUEST_METHOD_EMPTY;
    return false;
  }

  for (unsigned char c : d_request.method) {
    if (!std::isupper(c)) {
      d_error = ParseError::REQUEST_METHOD_LOWERCASE;
      return false;
    }
  }

  if (!Config::get().kAllowedMethods.contains(d_request.method)) {
    d_error = ParseError::UNSUPPORTED_METHOD;
    return false;
  }

  if (!Config::get().kAllowedVersions.contains(d_request.version)) {
    d_error = ParseError::UNSUPPORTED_VERSION;
    return false;
  }

  return true;
}

bool HttpParser::parseHeaders(std::string_view& buffer) {
  while (true) {
    if (!consumeLine(buffer, d_lineBuffer)) return false;

    if (d_lineBuffer.empty()) {
      return validateHeaders();
    }

    if (d_lineBuffer.size() > Config::get().kMaxHeaderBytes) {
      d_error = ParseError::HEADER_TOO_LARGE;
      d_state = ParseState::ERROR;
      return false;
    }

    auto colon = d_lineBuffer.find(':');
    if (colon == std::string::npos) {
      d_error = ParseError::INVALID_HEADER_MISSING_COLON;
      d_state = ParseState::ERROR;
      return false;
    }

    std::string name = d_lineBuffer.substr(0, colon);
    std::string value = d_lineBuffer.substr(colon + 1);

    for (unsigned char c : name) {
      if (!(std::isalnum(c) || c == '-')) {
        d_error = ParseError::INVALID_HEADER_UNSUPPORTED_CHARACTER;
        d_state = ParseState::ERROR;
        return false;
      }
    }

    while (!value.empty() && std::isspace((unsigned char)value.front()))
      value.erase(value.begin());

    d_currentHeaderCount++;
    if (d_currentHeaderCount > Config::get().kMaxHeaderCount) {
      d_error = ParseError::TOO_MANY_HEADERS;
      d_state = ParseState::ERROR;
      return false;
    }

    d_currentTotalHeaderSize += name.size() + value.size();
    if (d_currentTotalHeaderSize > Config::get().kMaxTotalHeaderSize) {
      d_error = ParseError::TOTAL_HEADER_SIZE_TOO_LARGE;
      d_state = ParseState::ERROR;
      return false;
    }

    d_request.headers[name].push_back(value);
  }
}

bool HttpParser::validateHeaders() {
  if (d_request.version == "HTTP/1.1") {
    auto host = d_request.headers.find("Host");
    if (host == d_request.headers.end() || host->second.empty() ||
        host->second.back().empty() || host->second.size() != 1) {
      d_error = ParseError::MISSING_HOST_HEADER;
      d_state = ParseState::ERROR;
      return false;
    }
  }

  auto cl = d_request.headers.find("Content-Length");
  if (cl != d_request.headers.end()) {
    if (cl->second.empty()) {
      d_error = ParseError::CONTENT_LENGTH_EMPTY;
      d_state = ParseState::ERROR;
      return false;
    }
    char* end = nullptr;
    std::strtoul(cl->second.back().c_str(), &end, 10);
    if (end == cl->second.back().c_str() || *end != '\0') {
      d_error = ParseError::CONTENT_LENGTH_NOT_NUMERIC;
      d_state = ParseState::ERROR;
      return false;
    }
  }

  return true;
}

bool HttpParser::determineBodyFraming() {
  auto te = d_request.headers.find("Transfer-Encoding");
  auto cl = d_request.headers.find("Content-Length");

  if (te != d_request.headers.end()) {
    if (!iequals(te->second.back(), "chunked")) {
      d_error = ParseError::INVALID_CHUNKED_ENCODING;
      d_state = ParseState::ERROR;
      return false;
    }
    d_bodyType = BodyType::CHUNKED;
    d_chunkState = ChunkState::SIZE;
    return true;
  }

  if (cl != d_request.headers.end()) {
    d_bodyBytesRemaining = std::strtoul(cl->second.back().c_str(), nullptr, 10);
    if (d_bodyBytesRemaining > Config::get().kMaxBodyBytes) {
      d_error = ParseError::BODY_TOO_LARGE;
      d_state = ParseState::ERROR;
      return false;
    }
    d_bodyType = BodyType::CONTENT_LENGTH;
    return true;
  }

  d_bodyType = BodyType::NONE;
  return true;
}

bool HttpParser::parseBody(std::string_view& buffer) {
  if (d_bodyType == BodyType::CONTENT_LENGTH) {
    size_t n = std::min(buffer.size(), d_bodyBytesRemaining);
    d_request.body.append(buffer.substr(0, n));
    buffer.remove_prefix(n);
    d_bodyBytesRemaining -= n;
    return d_bodyBytesRemaining == 0;
  }

  if (d_bodyType == BodyType::CHUNKED) {
    while (true) {
      switch (d_chunkState) {
        case ChunkState::SIZE:
          if (!parseChunkSize(buffer)) return false;
          break;
        case ChunkState::DATA:
          if (!parseChunkData(buffer)) return false;
          break;
        case ChunkState::DATA_CRLF:
          if (buffer.size() < 2) return false;
          if (buffer.substr(0, 2) != "\r\n") {
            d_error = ParseError::INVALID_CHUNKED_ENCODING;
            d_state = ParseState::ERROR;
            return false;
          }
          buffer.remove_prefix(2);
          d_chunkState = ChunkState::SIZE;
          break;
        case ChunkState::TRAILERS:
          if (!parseChunkTrailers(buffer)) return false;
          d_chunkState = ChunkState::DONE;
          return true;
        case ChunkState::DONE:
          return true;
      }
    }
  }

  return true;
}

bool HttpParser::parseChunkSize(std::string_view& buffer) {
  if (!consumeLine(buffer, d_lineBuffer)) return false;

  char* end = nullptr;
  d_currentChunkSize = std::strtoul(d_lineBuffer.c_str(), &end, 16);

  if (end == d_lineBuffer.c_str()) {
    d_error = ParseError::INVALID_CHUNKED_ENCODING;
    d_state = ParseState::ERROR;
    return false;
  }

  d_currentChunkRead = 0;

  if (d_currentChunkSize == 0) {
    d_chunkState = ChunkState::TRAILERS;
  } else {
    d_chunkState = ChunkState::DATA;
  }
  return true;
}

bool HttpParser::parseChunkData(std::string_view& buffer) {
  size_t remaining = d_currentChunkSize - d_currentChunkRead;
  size_t n = std::min(buffer.size(), remaining);

  d_request.body.append(buffer.substr(0, n));
  buffer.remove_prefix(n);
  d_currentChunkRead += n;

  if (d_currentChunkRead < d_currentChunkSize) {
    return false;
  }

  d_chunkState = ChunkState::DATA_CRLF;
  return true;
}

bool HttpParser::parseChunkTrailers(std::string_view& buffer) {
  while (true) {
    if (!consumeLine(buffer, d_lineBuffer)) return false;
    if (d_lineBuffer.empty()) return true;
  }
}

bool HttpParser::consumeLine(std::string_view& buffer, std::string& out) {
  size_t pos = buffer.find("\r\n");
  if (pos == std::string_view::npos) {
    return false;
  }
  out.assign(buffer.substr(0, pos));
  buffer.remove_prefix(pos + 2);
  return true;
}

std::string HttpParser::urlDecode(const std::string& s) {
  std::string out;
  out.reserve(s.size());

  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == '%' && i + 2 < s.size()) {
      char hex[3] = {s[i + 1], s[i + 2], 0};
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

void HttpParser::parseQueryParams(
    const std::string& qs, std::unordered_map<std::string, std::string>& map) {
  size_t start = 0;

  while (start < qs.size()) {
    size_t amp = qs.find('&', start);
    if (amp == std::string::npos) amp = qs.size();

    std::string pair = qs.substr(start, amp - start);

    size_t eq = pair.find('=');
    if (eq != std::string::npos) {
      std::string key = urlDecode(pair.substr(0, eq));
      std::string val = urlDecode(pair.substr(eq + 1));
      map[key] = val;
    } else {
      // key with no value
      map[urlDecode(pair)] = "";
    }

    start = amp + 1;
  }
}

}  // namespace HTTPServer