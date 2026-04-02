#include <gtest/gtest.h>
#include <httpserver_impl/http/http_object.h>
#include <httpserver_impl/http/http_parser.h>
#include <httpserver_impl/utils/utils.h>

using namespace HTTPServer;

TEST(HttpParserTests, EmptyRequest) {
  // GIVEN
  HttpParser parser;
  std::string recvBuffer;
  std::string_view view(recvBuffer);

  // WHEN
  ParseResult result = parser.parse(view);

  // THEN
  EXPECT_EQ(result, ParseResult::NEED_MORE_DATA);
  EXPECT_TRUE(view.empty());
}

TEST(HttpParserTests, EmptyMethod) {
  // GIVEN
  HttpParser parser;
  std::string recvBuffer = " / HTTP/1.1\r\nHost: x\r\n\r\n";
  std::string_view view(recvBuffer);

  // WHEN
  ParseResult result = parser.parse(view);

  // THEN
  EXPECT_EQ(result, ParseResult::PARSE_ERROR);

  ParseError err = parser.error();
  EXPECT_EQ(err, ParseError::REQUEST_METHOD_EMPTY);
}

TEST(HttpParserTests, EmptyPath) {
  // GIVEN
  HttpParser parser;
  std::string recvBuffer = "GET  HTTP/1.1\r\nHost: x\r\n\r\n";
  std::string_view view(recvBuffer);

  // WHEN
  ParseResult result = parser.parse(view);

  // THEN
  EXPECT_EQ(result, ParseResult::PARSE_ERROR);

  ParseError err = parser.error();
  EXPECT_EQ(parser.error(), ParseError::INVALID_REQUEST_LINE_FORMAT);
}

TEST(HttpParserTests, ExtraSpacesInRequestLine) {
  // GIVEN
  HttpParser parser;
  std::string recvBuffer = "GET  /  HTTP/1.1\r\nHost: x\r\n\r\n";
  std::string_view view(recvBuffer);

  // WHEN
  ParseResult result = parser.parse(view);

  // THEN
  EXPECT_EQ(result, ParseResult::PARSE_ERROR);

  ParseError err = parser.error();
  EXPECT_EQ(err, ParseError::INVALID_REQUEST_LINE_FORMAT);
}

TEST(HttpParserTests, PartialRequestNeedsMoreData) {
  // GIVEN
  HttpParser parser;
  std::string recvBuffer = "GET / HTTP/1.1\r\n";
  std::string_view view(recvBuffer);

  // WHEN
  ParseResult result = parser.parse(view);

  // THEN
  EXPECT_EQ(result, ParseResult::NEED_MORE_DATA);
}

TEST(HttpParserTests, PipelinedRequests) {
  // GIVEN
  HttpParser parser;
  std::string recvBuffer =
      "GET /a HTTP/1.1\r\nHost: x\r\n\r\n"
      "GET /b HTTP/1.1\r\nHost: y\r\n\r\n";

  std::string_view view(recvBuffer);

  // WHEN and THEN
  ParseResult r1 = parser.parse(view);
  EXPECT_EQ(r1, ParseResult::REQUEST_COMPLETE);
  HttpRequest req1 = parser.takeRequest();
  EXPECT_EQ(req1.path, "/a");

  ParseResult r2 = parser.parse(view);
  EXPECT_EQ(r2, ParseResult::REQUEST_COMPLETE);
  HttpRequest req2 = parser.takeRequest();
  EXPECT_EQ(req2.path, "/b");

  EXPECT_TRUE(view.empty());
}

TEST(HttpParserTests, ValidGetRequest) {
  // GIVEN
  HttpParser parser;
  const std::string recvBuffer =
      "GET /index.html HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "User-Agent: TestClient\r\n"
      "\r\n";

  std::string_view view(recvBuffer);

  // WHEN
  ParseResult result = parser.parse(view);

  // THEN
  EXPECT_EQ(result, ParseResult::REQUEST_COMPLETE);

  ParseError err = parser.error();
  EXPECT_EQ(err, ParseError::NONE);

  HttpRequest req = parser.takeRequest();
  EXPECT_EQ(req.method, "GET");
  EXPECT_EQ(req.path, "/index.html");
  EXPECT_EQ(req.version, "HTTP/1.1");

  EXPECT_EQ(req.headers["host"][0], "localhost");
  EXPECT_EQ(req.headers["user-agent"][0], "TestClient");
}

TEST(HttpParserTests, InvalidRequestLineFormat) {
  // GIVEN
  HttpParser parser;
  const std::string recvBuffer =
      "BADREQUESTLINE\r\n"
      "\r\n";

  std::string_view view(recvBuffer);

  // WHEN
  ParseResult result = parser.parse(view);

  // THEN
  EXPECT_EQ(result, ParseResult::PARSE_ERROR);

  ParseError err = parser.error();
  EXPECT_EQ(err, ParseError::INVALID_REQUEST_LINE_FORMAT);
}

TEST(HttpParserTests, InvalidMethod) {
  // GIVEN
  HttpParser parser;
  const std::string recvBuffer =
      "BADREQUESTLINE / HTTP1.1\r\n"
      "\r\n";

  std::string_view view(recvBuffer);

  // WHEN
  ParseResult result = parser.parse(view);

  // THEN
  EXPECT_EQ(result, ParseResult::PARSE_ERROR);

  ParseError err = parser.error();
  EXPECT_EQ(err, ParseError::UNSUPPORTED_METHOD);
}

TEST(HttpParserTests, InvalidMethodFormat) {
  // GIVEN
  HttpParser parser;
  const std::string recvBuffer =
      "get / HTTP/1.1\r\n"
      "Host: x\r\n"
      "\r\n";

  std::string_view view(recvBuffer);

  // WHEN
  ParseResult result = parser.parse(view);

  // THEN
  EXPECT_EQ(result, ParseResult::PARSE_ERROR);

  ParseError err = parser.error();
  EXPECT_EQ(err, ParseError::REQUEST_METHOD_LOWERCASE);
}

TEST(HttpParserTests, InvalidVersion) {
  // GIVEN
  HttpParser parser;
  const std::string recvBuffer =
      "GET / HTTP/2.0\r\n"
      "Host: x\r\n"
      "\r\n";

  std::string_view view(recvBuffer);

  // WHEN
  ParseResult result = parser.parse(view);

  // THEN
  EXPECT_EQ(result, ParseResult::PARSE_ERROR);

  ParseError err = parser.error();
  EXPECT_EQ(err, ParseError::UNSUPPORTED_VERSION);
}

TEST(HttpParserTests, InvalidHeaderFormat) {
  // GIVEN
  HttpParser parser;
  const std::string recvBuffer =
      "GET / HTTP/1.1\r\n"
      "HeaderWithoutColon\r\n"
      "\r\n";

  std::string_view view(recvBuffer);

  // WHEN
  ParseResult result = parser.parse(view);

  // THEN
  EXPECT_EQ(result, ParseResult::PARSE_ERROR);

  ParseError err = parser.error();
  EXPECT_EQ(err, ParseError::INVALID_HEADER_MISSING_COLON);
}

TEST(HttpParserTests, MissingHostHeaderHTTP11) {
  // GIVEN
  HttpParser parser;
  std::string recvBuffer = "GET / HTTP/1.1\r\n\r\n";
  std::string_view view(recvBuffer);

  // WHEN
  ParseResult result = parser.parse(view);

  // THEN
  EXPECT_EQ(result, ParseResult::PARSE_ERROR);

  ParseError err = parser.error();
  EXPECT_EQ(err, ParseError::MISSING_HOST_HEADER);
}

TEST(HttpParserTests, EmptyHostHeader) {
  // GIVEN
  HttpParser parser;
  std::string recvBuffer = "GET / HTTP/1.1\r\nHost:\r\n\r\n";
  std::string_view view(recvBuffer);

  // WHEN
  ParseResult result = parser.parse(view);

  // THEN
  EXPECT_EQ(result, ParseResult::PARSE_ERROR);

  ParseError err = parser.error();
  EXPECT_EQ(err, ParseError::MISSING_HOST_HEADER);
}

TEST(HttpParserTests, DuplicateHostHeaders) {
  // GIVEN
  HttpParser parser;
  std::string recvBuffer =
      "GET / HTTP/1.1\r\n"
      "Host: a\r\n"
      "Host: b\r\n"
      "\r\n";
  std::string_view view(recvBuffer);

  // WHEN
  ParseResult result = parser.parse(view);

  // THEN
  EXPECT_EQ(result, ParseResult::PARSE_ERROR);

  ParseError err = parser.error();
  EXPECT_EQ(err, ParseError::MISSING_HOST_HEADER);
}

TEST(HttpParserTests, InvalidContentLength) {
  // GIVEN
  HttpParser parser;
  std::string recvBuffer =
      "POST / HTTP/1.1\r\n"
      "Host: x\r\n"
      "Content-Length: abc\r\n"
      "\r\n";
  std::string_view view(recvBuffer);

  // WHEN
  ParseResult result = parser.parse(view);

  // THEN
  EXPECT_EQ(result, ParseResult::PARSE_ERROR);

  ParseError err = parser.error();
  EXPECT_EQ(err, ParseError::CONTENT_LENGTH_NOT_NUMERIC);
}

TEST(HttpParserTests, ContentLengthTooShort) {
  // GIVEN
  HttpParser parser;
  std::string recvBuffer =
      "POST / HTTP/1.1\r\n"
      "Host: x\r\n"
      "Content-Length: 10\r\n"
      "\r\n"
      "abc";

  std::string_view view(recvBuffer);

  // WHEN
  ParseResult result = parser.parse(view);

  // THEN
  EXPECT_EQ(result, ParseResult::NEED_MORE_DATA);

  ParseError err = parser.error();
  EXPECT_EQ(err, ParseError::NONE);
}

TEST(HttpParserTests, ContentLengthTooLarge) {
  // GIVEN
  HttpParser parser;
  std::string recvBuffer =
      "POST / HTTP/1.1\r\n"
      "Host: x\r\n"
      "Content-Length: 999999999\r\n"
      "\r\n";

  std::string_view view(recvBuffer);

  // WHEN
  ParseResult result = parser.parse(view);

  // THEN
  EXPECT_EQ(result, ParseResult::PARSE_ERROR);

  ParseError err = parser.error();
  EXPECT_EQ(err, ParseError::BODY_TOO_LARGE);
}

TEST(HttpParserTests, InvalidHeaderName) {
  // GIVEN
  HttpParser parser;
  const std::string recvBuffer =
      "GET / HTTP/1.1\r\n"
      "Bad Header: value\r\n"
      "\r\n";

  std::string_view view(recvBuffer);

  // WHEN
  ParseResult result = parser.parse(view);

  // THEN
  EXPECT_EQ(result, ParseResult::PARSE_ERROR);

  ParseError err = parser.error();
  EXPECT_EQ(err, ParseError::INVALID_HEADER_UNSUPPORTED_CHARACTER);
}

TEST(HttpParserTests, BodyParsing) {
  // GIVEN
  HttpParser parser;
  const std::string recvBuffer =
      "POST /submit HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Content-Length: 11\r\n"
      "\r\n"
      "hello world";

  std::string_view view(recvBuffer);

  // WHEN
  ParseResult result = parser.parse(view);

  // THEN
  EXPECT_EQ(result, ParseResult::REQUEST_COMPLETE);

  ParseError err = parser.error();
  EXPECT_EQ(err, ParseError::NONE);

  HttpRequest req = parser.takeRequest();
  EXPECT_EQ(req.method, "POST");
  EXPECT_EQ(req.path, "/submit");
  EXPECT_EQ(req.version, "HTTP/1.1");
  EXPECT_EQ(req.body, "hello world");
}

TEST(HttpParserTests, ChunkedBodySimple) {
  // GIVEN
  HttpParser parser;
  std::string recvBuffer =
      "POST / HTTP/1.1\r\n"
      "Host: x\r\n"
      "Transfer-Encoding: chunked\r\n"
      "\r\n"
      "5\r\nhello\r\n"
      "0\r\n\r\n";

  std::string_view view(recvBuffer);

  // WHEN
  ParseResult result = parser.parse(view);

  // THEN
  EXPECT_EQ(result, ParseResult::REQUEST_COMPLETE);

  ParseError err = parser.error();
  EXPECT_EQ(err, ParseError::NONE);

  HttpRequest req = parser.takeRequest();
  EXPECT_EQ(req.body, "hello");
}

TEST(HttpParserTests, ChunkedInvalidSize) {
  // GIVEN
  HttpParser parser;
  std::string recvBuffer =
      "POST / HTTP/1.1\r\n"
      "Host: x\r\n"
      "Transfer-Encoding: chunked\r\n"
      "\r\n"
      "ZZZ\r\n";

  std::string_view view(recvBuffer);

  // WHEN
  ParseResult result = parser.parse(view);

  // THEN
  EXPECT_EQ(result, ParseResult::PARSE_ERROR);

  ParseError err = parser.error();
  EXPECT_EQ(err, ParseError::INVALID_CHUNKED_ENCODING);
}

TEST(HttpParserTests, ChunkedMissingCRLF) {
  // GIVEN
  HttpParser parser;
  std::string recvBuffer =
      "POST / HTTP/1.1\r\n"
      "Host: x\r\n"
      "Transfer-Encoding: chunked\r\n"
      "\r\n"
      "5\r\nhello"
      "0\r\n\r\n";

  std::string_view view(recvBuffer);

  // WHEN
  ParseResult result = parser.parse(view);

  // THEN
  EXPECT_EQ(result, ParseResult::PARSE_ERROR);

  ParseError err = parser.error();
  EXPECT_EQ(err, ParseError::INVALID_CHUNKED_ENCODING);
}

TEST(HttpParserTests, TransferEncodingOverridesContentLength) {
  // GIVEN
  HttpParser parser;
  std::string recvBuffer =
      "POST / HTTP/1.1\r\n"
      "Host: x\r\n"
      "Content-Length: 5\r\n"
      "Transfer-Encoding: chunked\r\n"
      "\r\n"
      "5\r\nhello\r\n0\r\n\r\n";

  std::string_view view(recvBuffer);

  // WHEN
  ParseResult result = parser.parse(view);

  // THEN
  EXPECT_EQ(result, ParseResult::REQUEST_COMPLETE);

  ParseError err = parser.error();
  EXPECT_EQ(err, ParseError::NONE);
}

TEST(HttpParserTests, ChunkedBodyMultipleChunks) {
  // GIVEN
  HttpParser parser;
  std::string recvBuffer =
      "POST / HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Transfer-Encoding: chunked\r\n"
      "\r\n"
      "5\r\n"
      "hello\r\n"
      "6\r\n"
      " world\r\n"
      "0\r\n"
      "\r\n";

  std::string_view view(recvBuffer);

  // WHEN
  ParseResult result = parser.parse(view);

  // THEN
  ASSERT_EQ(result, ParseResult::REQUEST_COMPLETE);

  HttpRequest req = parser.takeRequest();
  EXPECT_EQ(req.body, "hello world");
}

TEST(HttpParserTests, ChunkedBodyNeedsMoreData) {
  // GIVEN
  HttpParser parser;
  std::string part1 =
      "POST / HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Transfer-Encoding: chunked\r\n"
      "\r\n"
      "5\r\nhel";
  std::string_view view1(part1);

  std::string part2 = "lo\r\n0\r\n\r\n";
  std::string_view view2(part2);

  // WHEN and THEN
  ParseResult result1 = parser.parse(view1);
  EXPECT_EQ(result1, ParseResult::NEED_MORE_DATA);

  ParseError err1 = parser.error();
  EXPECT_EQ(err1, ParseError::NONE);

  ParseResult result2 = parser.parse(view2);
  EXPECT_EQ(result2, ParseResult::REQUEST_COMPLETE);

  ParseError err2 = parser.error();
  EXPECT_EQ(err2, ParseError::NONE);

  HttpRequest req = parser.takeRequest();
  EXPECT_EQ(req.body, "hello");
}

TEST(HttpParserTests, ChunkedMissingFinalZeroChunk) {
  // GIVEN
  HttpParser parser;
  std::string recvBuffer =
      "POST / HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Transfer-Encoding: chunked\r\n"
      "\r\n"
      "5\r\n"
      "hello\r\n";

  std::string_view view(recvBuffer);

  // WHEN
  ParseResult result = parser.parse(view);

  // THEN
  EXPECT_EQ(result, ParseResult::NEED_MORE_DATA);

  ParseError err = parser.error();
  EXPECT_EQ(err, ParseError::NONE);
}

TEST(HttpParserTests, ChunkedWithExtensions) {
  // GIVEN
  HttpParser parser;
  std::string recvBuffer =
      "POST / HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Transfer-Encoding: chunked\r\n"
      "\r\n"
      "5;ext=value\r\n"
      "hello\r\n"
      "0\r\n"
      "\r\n";

  std::string_view view(recvBuffer);

  // WHEN
  ParseResult result = parser.parse(view);

  // THEN
  ASSERT_EQ(result, ParseResult::REQUEST_COMPLETE);

  HttpRequest req = parser.takeRequest();
  EXPECT_EQ(req.body, "hello");
}

TEST(HttpParserTests, ChunkedWithTrailers) {
  // GIVEN
  HttpParser parser;
  std::string recvBuffer =
      "POST / HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Transfer-Encoding: chunked\r\n"
      "\r\n"
      "5\r\n"
      "hello\r\n"
      "0\r\n"
      "X-Trailer: value\r\n"
      "\r\n";

  std::string_view view(recvBuffer);

  // WHEN
  ParseResult result = parser.parse(view);

  // THEN
  ASSERT_EQ(result, ParseResult::REQUEST_COMPLETE);

  HttpRequest req = parser.takeRequest();
  EXPECT_EQ(req.body, "hello");
}

TEST(HttpParserTests, QueryStringSimple) {
  // GIVEN
  HttpParser parser;
  const std::string recvBuffer =
      "GET /search?q=hello&page=2 HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "\r\n";

  std::string_view view(recvBuffer);

  // WHEN
  ParseResult result = parser.parse(view);

  // THEN
  ASSERT_EQ(result, ParseResult::REQUEST_COMPLETE);

  ParseError err = parser.error();
  EXPECT_EQ(err, ParseError::NONE);

  HttpRequest req = parser.takeRequest();
  EXPECT_EQ(req.path, "/search");
  EXPECT_EQ(req.params["q"], "hello");
  EXPECT_EQ(req.params["page"], "2");
}

TEST(HttpParserTests, QueryStringURLDecoding) {
  // GIVEN
  HttpParser parser;
  const std::string recvBuffer =
      "GET /find?term=hello%20world%21&x=%2Fpath%2F HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "\r\n";

  std::string_view view(recvBuffer);

  // WHEN
  ParseResult result = parser.parse(view);

  // THEN
  ASSERT_EQ(result, ParseResult::REQUEST_COMPLETE);

  ParseError err = parser.error();
  EXPECT_EQ(err, ParseError::NONE);

  HttpRequest req = parser.takeRequest();
  EXPECT_EQ(req.path, "/find");
  EXPECT_EQ(req.params["term"], "hello world!");
  EXPECT_EQ(req.params["x"], "/path/");
}

TEST(HttpParserTests, QueryStringEmptyKeyOrValue) {
  // GIVEN
  HttpParser parser;
  const std::string recvBuffer =
      "GET /test?empty=&alsoempty HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "\r\n";

  std::string_view view(recvBuffer);

  // WHEN
  ParseResult result = parser.parse(view);

  // THEN
  ASSERT_EQ(result, ParseResult::REQUEST_COMPLETE);

  ParseError err = parser.error();
  EXPECT_EQ(err, ParseError::NONE);

  HttpRequest req = parser.takeRequest();
  EXPECT_EQ(req.path, "/test");
  EXPECT_EQ(req.params["empty"], "");
  EXPECT_EQ(req.params["alsoempty"], "");
}

TEST(HttpParserTests, QueryStringNoParamsAfterQuestionMark) {
  // GIVEN
  HttpParser parser;
  const std::string recvBuffer =
      "GET /page? HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "\r\n";

  std::string_view view(recvBuffer);

  // WHEN
  ParseResult result = parser.parse(view);

  // THEN
  ASSERT_EQ(result, ParseResult::REQUEST_COMPLETE);

  ParseError err = parser.error();
  EXPECT_EQ(err, ParseError::NONE);

  HttpRequest req = parser.takeRequest();
  EXPECT_EQ(req.path, "/page");
  EXPECT_TRUE(req.params.empty());
}

TEST(HttpParserTests, QueryStringNoQuestionMark) {
  // GIVEN
  HttpParser parser;
  const std::string recvBuffer =
      "GET /plain HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "\r\n";

  std::string_view view(recvBuffer);

  // WHEN
  ParseResult result = parser.parse(view);

  // THEN
  ASSERT_EQ(result, ParseResult::REQUEST_COMPLETE);

  ParseError err = parser.error();
  EXPECT_EQ(err, ParseError::NONE);

  HttpRequest req = parser.takeRequest();
  EXPECT_EQ(req.path, "/plain");
  EXPECT_TRUE(req.params.empty());
}

TEST(HttpParserTests, QueryStringDuplicateKeys) {
  // GIVEN
  HttpParser parser;
  const std::string recvBuffer =
      "GET /dup?a=1&a=2 HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "\r\n";

  std::string_view view(recvBuffer);

  // WHEN
  ParseResult result = parser.parse(view);

  // THEN
  ASSERT_EQ(result, ParseResult::REQUEST_COMPLETE);

  ParseError err = parser.error();
  EXPECT_EQ(err, ParseError::NONE);

  HttpRequest req = parser.takeRequest();
  EXPECT_EQ(req.path, "/dup");
  EXPECT_EQ(req.params["a"], "2");
}

TEST(HttpParserTests, QueryStringUTF8Decoding) {
  // GIVEN
  HttpParser parser;
  const std::string recvBuffer =
      "GET /emoji?q=%F0%9F%98%80 HTTP/1.1\r\n"  // UTF-8 😀
      "Host: localhost\r\n"
      "\r\n";

  std::string_view view(recvBuffer);

  // WHEN
  ParseResult result = parser.parse(view);

  // THEN
  ASSERT_EQ(result, ParseResult::REQUEST_COMPLETE);

  ParseError err = parser.error();
  EXPECT_EQ(err, ParseError::NONE);

  HttpRequest req = parser.takeRequest();
  EXPECT_EQ(req.path, "/emoji");
  EXPECT_EQ(req.params["q"], "😀");
}

TEST(HttpParserTests, ResetClearsState) {
  // GIVEN
  HttpParser parser;
  std::string recvBuffer = "GET / HTTP/1.1\r\nHost: x\r\n\r\n";
  std::string_view view(recvBuffer);

  // WHEN and THEN
  ASSERT_EQ(parser.parse(view), ParseResult::REQUEST_COMPLETE);

  parser.reset();

  std::string recvBuffer2 = "GET /b HTTP/1.1\r\nHost: y\r\n\r\n";
  std::string_view view2(recvBuffer2);

  EXPECT_EQ(parser.parse(view2), ParseResult::REQUEST_COMPLETE);
  EXPECT_EQ(parser.takeRequest().path, "/b");
}

TEST(HttpParserTests, UnexpectedEOFInHeaders) {
  // GIVEN
  HttpParser parser;
  std::string recvBuffer = "GET / HTTP/1.1\r\nHost:";
  std::string_view view(recvBuffer);

  // WHEN
  ParseResult result = parser.parse(view);

  // THEN
  EXPECT_EQ(result, ParseResult::NEED_MORE_DATA);

  ParseError err = parser.error();
  EXPECT_EQ(err, ParseError::NONE);
}

TEST(HttpParserTests, TooManyHeaders) {
  // GIVEN
  HttpParser parser;
  std::string recvBuffer = "GET / HTTP/1.1\r\nHost: x\r\n";
  for (int i = 0; i < 101; ++i) {
    recvBuffer += "X-Header-" + std::to_string(i) + ": value\r\n";
  }
  recvBuffer += "\r\n";
  std::string_view view(recvBuffer);

  // WHEN
  ParseResult result = parser.parse(view);

  // THEN
  EXPECT_EQ(result, ParseResult::PARSE_ERROR);
  EXPECT_EQ(parser.error(), ParseError::TOO_MANY_HEADERS);
}

TEST(HttpParserTests, TotalHeaderSizeTooLarge) {
  // GIVEN
  HttpParser parser;
  std::string recvBuffer = "GET / HTTP/1.1\r\nHost: x\r\n";
  // kMaxHeaderBytes is 16KB. kMaxTotalHeaderSize is 64KB.
  // We send 5 headers of 15KB each to exceed 64KB total.
  std::string value(15 * 1024, 'a');
  for (int i = 0; i < 5; ++i) {
    recvBuffer += "X-Header-" + std::to_string(i) + ": " + value + "\r\n";
  }
  recvBuffer += "\r\n";
  std::string_view view(recvBuffer);

  // WHEN
  ParseResult result = parser.parse(view);

  // THEN
  EXPECT_EQ(result, ParseResult::PARSE_ERROR);
  EXPECT_EQ(parser.error(), ParseError::TOTAL_HEADER_SIZE_TOO_LARGE);
}

TEST(HttpParserTests, CaseInsensitiveHeaders) {
  // GIVEN
  HttpParser parser;
  const std::string recvBuffer =
      "GET / HTTP/1.1\r\n"
      "host: localhost\r\n"
      "CONTENT-LENGTH: 0\r\n"
      "X-Custom-Header: value\r\n"
      "\r\n";

  std::string_view view(recvBuffer);

  // WHEN
  ParseResult result = parser.parse(view);

  // THEN
  EXPECT_EQ(result, ParseResult::REQUEST_COMPLETE);
  EXPECT_EQ(parser.error(), ParseError::NONE);

  HttpRequest req = parser.takeRequest();
  EXPECT_EQ(req.headers.at("host")[0], "localhost");
  EXPECT_EQ(req.headers.at("content-length")[0], "0");
  EXPECT_EQ(req.headers.at("x-custom-header")[0], "value");
}

TEST(HttpParserTests, UtilsCaseInsensitive) {
  // GIVEN
  HttpParser parser;
  const std::string recvBuffer =
      "GET / HTTP/1.1\r\n"
      "CONNECTION: keep-alive\r\n"
      "HOST: example.com\r\n"
      "\r\n";

  std::string_view view(recvBuffer);

  // WHEN
  ParseResult result = parser.parse(view);

  // THEN
  EXPECT_EQ(result, ParseResult::REQUEST_COMPLETE);
  HttpRequest req = parser.takeRequest();

  EXPECT_TRUE(requestWantsKeepAlive(req));
  auto host = get_last_header_value(req, "Host");
  ASSERT_TRUE(host.has_value());
  EXPECT_EQ(*host, "example.com");

  auto hostLower = get_last_header_value(req, "host");
  ASSERT_TRUE(hostLower.has_value());
  EXPECT_EQ(*hostLower, "example.com");
}
