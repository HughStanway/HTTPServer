#include <gtest/gtest.h>

#include <httpserver/http_object.h>
#include <httpserver/http_parser.h>

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

TEST(HttpParserTests, QueryStringSimple) {
    const std::string raw = "GET /search?q=hello&page=2 HTTP/1.1\r\n"
                            "Host: localhost\r\n"
                            "\r\n";

    HttpRequest req;
    ParseError err = HttpParser::parse(raw, req);

    EXPECT_EQ(err, ParseError::NONE);
    EXPECT_EQ(req.path, "/search");
    EXPECT_EQ(req.params["q"], "hello");
    EXPECT_EQ(req.params["page"], "2");
}

TEST(HttpParserTests, QueryStringURLDecoding) {
    const std::string raw = "GET /find?term=hello%20world%21&x=%2Fpath%2F HTTP/1.1\r\n"
                            "Host: localhost\r\n"
                            "\r\n";

    HttpRequest req;
    ParseError err = HttpParser::parse(raw, req);

    EXPECT_EQ(err, ParseError::NONE);
    EXPECT_EQ(req.path, "/find");
    EXPECT_EQ(req.params["term"], "hello world!");
    EXPECT_EQ(req.params["x"], "/path/");
}

TEST(HttpParserTests, QueryStringEmptyKeyOrValue) {
    const std::string raw = "GET /test?empty=&alsoempty HTTP/1.1\r\n"
                            "Host: localhost\r\n"
                            "\r\n";

    HttpRequest req;
    ParseError err = HttpParser::parse(raw, req);

    EXPECT_EQ(err, ParseError::NONE);
    EXPECT_EQ(req.path, "/test");

    EXPECT_EQ(req.params["empty"], "");
    EXPECT_EQ(req.params["alsoempty"], "");
}

TEST(HttpParserTests, QueryStringNoParamsAfterQuestionMark) {
    const std::string raw = "GET /page? HTTP/1.1\r\n"
                            "Host: localhost\r\n"
                            "\r\n";

    HttpRequest req;
    ParseError err = HttpParser::parse(raw, req);

    EXPECT_EQ(err, ParseError::NONE);
    EXPECT_EQ(req.path, "/page");
    EXPECT_TRUE(req.params.empty());
}

TEST(HttpParserTests, QueryStringNoQuestionMark) {
    const std::string raw = "GET /plain HTTP/1.1\r\n"
                            "Host: localhost\r\n"
                            "\r\n";

    HttpRequest req;
    ParseError err = HttpParser::parse(raw, req);

    EXPECT_EQ(err, ParseError::NONE);
    EXPECT_EQ(req.path, "/plain");
    EXPECT_TRUE(req.params.empty());
}

TEST(HttpParserTests, QueryStringDuplicateKeys) {
    const std::string raw =
        "GET /dup?a=1&a=2 HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    HttpRequest req;
    ParseError err = HttpParser::parse(raw, req);

    EXPECT_EQ(err, ParseError::NONE);
    EXPECT_EQ(req.path, "/dup");
    EXPECT_EQ(req.params["a"], "2");
}

TEST(HttpParserTests, QueryStringUTF8Decoding) {
    const std::string raw =
        "GET /emoji?q=%F0%9F%98%80 HTTP/1.1\r\n" // UTF-8 😀
        "Host: localhost\r\n"
        "\r\n";

    HttpRequest req;
    ParseError err = HttpParser::parse(raw, req);

    EXPECT_EQ(err, ParseError::NONE);
    EXPECT_EQ(req.path, "/emoji");
    EXPECT_EQ(req.params["q"], "😀");
}