#include <gtest/gtest.h>

#include "http_object.h"
#include "http_parser.h"

using namespace HTTPServer;

TEST(HttpParserTests, EmptyRequest) {
    HttpRequest req;
    ParseError err = HttpParser::parse("", req);
    EXPECT_EQ(err, ParseError::EMPTY_REQUEST);
}

TEST(HttpParserTests, ValidGetRequest) {
    const std::string raw = "GET /index.html HTTP/1.1\r\n"
                            "Host: localhost\r\n"
                            "User-Agent: TestClient\r\n"
                            "\r\n";

    HttpRequest req;
    ParseError err = HttpParser::parse(raw, req);

    EXPECT_EQ(err, ParseError::NONE);
    EXPECT_EQ(req.method, "GET");
    EXPECT_EQ(req.path, "/index.html");
    EXPECT_EQ(req.version, "HTTP/1.1");

    EXPECT_EQ(req.headers["Host"], "localhost");
    EXPECT_EQ(req.headers["User-Agent"], "TestClient");
}

TEST(HttpParserTests, InvalidRequestLine) {
    const std::string raw = "BADREQUESTLINE\r\n"
                            "\r\n";

    HttpRequest req;
    ParseError err = HttpParser::parse(raw, req);
    EXPECT_EQ(err, ParseError::INVALID_REQUEST_LINE);
}

TEST(HttpParserTests, InvalidMethod) {
    const std::string raw = "get / HTTP/1.1\r\n"
                            "Host: x\r\n"
                            "\r\n";

    HttpRequest req;
    ParseError err = HttpParser::parse(raw, req);
    EXPECT_EQ(err, ParseError::INVALID_METHOD);
}

TEST(HttpParserTests, InvalidVersion) {
    const std::string raw = "GET / WRONGVERSION\r\n"
                            "Host: x\r\n"
                            "\r\n";

    HttpRequest req;
    ParseError err = HttpParser::parse(raw, req);
    EXPECT_EQ(err, ParseError::INVALID_VERSION);
}

TEST(HttpParserTests, InvalidHeaderFormat) {
    const std::string raw = "GET / HTTP/1.1\r\n"
                            "HeaderWithoutColon\r\n"
                            "\r\n";

    HttpRequest req;
    ParseError err = HttpParser::parse(raw, req);
    EXPECT_EQ(err, ParseError::INVALID_HEADER_FORMAT);
}

TEST(HttpParserTests, InvalidHeaderName) {
    const std::string raw = "GET / HTTP/1.1\r\n"
                            "Bad Header: value\r\n"
                            "\r\n";

    HttpRequest req;
    ParseError err = HttpParser::parse(raw, req);
    EXPECT_EQ(err, ParseError::INVALID_HEADER_NAME);
}

TEST(HttpParserTests, BodyParsing) {
    const std::string raw = "POST /submit HTTP/1.1\r\n"
                            "Host: localhost\r\n"
                            "Content-Length: 11\r\n"
                            "\r\n"
                            "hello world";

    HttpRequest req;
    ParseError err = HttpParser::parse(raw, req);

    EXPECT_EQ(err, ParseError::NONE);
    EXPECT_EQ(req.method, "POST");
    EXPECT_EQ(req.body, "hello world\n"); // parser adds newline
}
