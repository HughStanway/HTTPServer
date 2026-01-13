#ifndef HTTP_PARSER_V2_H
#define HTTP_PARSER_V2_H

#include "http_object.h"

namespace HTTPServer {

enum class ParseState { REQUEST_LINE, HEADERS, BODY, COMPLETE, ERROR };

enum class BodyType { NONE, CONTENT_LENGTH, CHUNKED };

enum class ChunkState { SIZE, DATA, DATA_CRLF, TRAILERS, DONE };

enum class ParseResult { NEED_MORE_DATA, REQUEST_COMPLETE, PARSE_ERROR };

enum class ParseError {
  NONE,
  INVALID_REQUEST_LINE_FORMAT,
  REQUEST_METHOD_EMPTY,
  REQUEST_METHOD_LOWERCASE,
  UNSUPPORTED_METHOD,
  UNSUPPORTED_VERSION,
  INVALID_HEADER_MISSING_COLON,
  INVALID_HEADER_UNSUPPORTED_CHARACTER,
  HEADER_TOO_LARGE,
  MISSING_HOST_HEADER,
  CONTENT_LENGTH_EMPTY,
  CONTENT_LENGTH_NOT_NUMERIC,
  BODY_TOO_LARGE,
  INVALID_CHUNKED_ENCODING,
};

class HttpParser {
 public:
  HttpParser();

  /**
   * Feed new bytes into the parser. Consumes as much as
   * possible from `buffer` and leaves any unconsumed
   * bytes in buffer
   */
  ParseResult parse(std::string_view& buffer);
  bool hasCompleteRequest() const;
  HttpRequest takeRequest();
  bool hasError() const;
  ParseError error() const;

  /**
   * Prepare parser for next request (pipelining)
   */
  void reset();

 private:
  ParseState d_state = ParseState::REQUEST_LINE;
  BodyType d_bodyType = BodyType::NONE;
  ChunkState d_chunkState = ChunkState::SIZE;
  ParseError d_error = ParseError::NONE;
  HttpRequest d_request;

  /**
   * Temporary Buffers
   */
  std::string d_lineBuffer;
  size_t d_bodyBytesRemaining = 0;
  size_t d_currentChunkSize = 0;

  /**
   * Limits (DoS protection)
   */
  size_t kMaxHeaderBytes = 16 * 1024;
  size_t kMaxBodyBytes = 10 * 1024 * 1024;

  /**
   * Internal parsing methods
   */
  bool parseRequestLine(std::string_view& buffer);
  bool parseHeaders(std::string_view& buffer);
  bool determineBodyFraming();
  bool parseBody(std::string_view& buffer);

  bool parseChunkSize(std::string_view& buffer);
  bool parseChunkData(std::string_view& buffer);
  bool parseChunkTrailers(std::string_view& buffer);

  bool consumeLine(std::string_view& buffer, std::string& out);
  bool validateRequestLine();
  bool validateHeaders();
  void parseQueryParams(const std::string& qs,
                        std::unordered_map<std::string, std::string>& map);
  std::string urlDecode(const std::string& s);
};

}  // namespace HTTPServer

#endif