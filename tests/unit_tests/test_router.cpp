#include <gtest/gtest.h>

#include <httpserver/http_object.h>
#include <httpserver/http_parser.h>
#include <httpserver/router.h>
#include <httpserver/http_object.h>
#include <httpserver/http_response_builder.h>

#include <filesystem>
#include <fstream>

using namespace HTTPServer;

static HttpRequest makeReq(const std::string& path) {
    HttpRequest req;
    req.method = "GET";
    req.path = path;
    req.version = "HTTP/1.1";
    return req;
}

TEST(RouterTests, StaticRouteExactMatch) {
    // GIVEN:
    Router::instance().addRoute("GET", "/hello", [](const HttpRequest& req) {
        return Responses::ok(req, "hi", "text/plain");
    });

    HttpRequest req = makeReq("/hello");

    // WHEN:
    HttpResponse res = Router::instance().route(req);

    // THEN:
    EXPECT_EQ(res.code, StatusCode::OK);
}

TEST(RouterTests, StaticRouteNotFoundWhenNoMatch) {
    // GIVEN
    Router::instance().addRoute("GET", "/exists", [](const HttpRequest& req) {
        return Responses::ok(req, "exists", "text/plain");
    });

    HttpRequest req = makeReq("/missing");

    // WHEN:
    auto res = Router::instance().route(req);

    // THEN;
    EXPECT_EQ(res.code, StatusCode::NotFound);
}

TEST(RouterTests, StaticRouteMethodMismatch) {
    // GIVEN:
    Router::instance().addRoute("GET", "/hello", [](const HttpRequest& req) {
        return Responses::ok(req, "exists", "text/plain");
    });

    HttpRequest req;
    req.method = "POST";
    req.path = "/hello";
    req.version = "HTTP/1.1";

    // WHEN:
    HttpResponse res = Router::instance().route(req);

    // THEN:
    EXPECT_EQ(res.code, StatusCode::NotFound);
}

TEST(RouterTests, StaticRouteOverwritesPrevious) {
    // GIVEN:
    Router::instance().addRoute("GET", "/path", [](const HttpRequest& req) {
        return Responses::ok(req, "first", "text/plain");
    });

    Router::instance().addRoute("GET", "/path", [](const HttpRequest& req) {
        return Responses::ok(req, "second", "text/plain");
    });

    HttpRequest req = makeReq("/path");

    // WHEN:
    HttpResponse res = Router::instance().route(req);

    // THEN:
    EXPECT_TRUE(res.serialize().find("second") != std::string::npos);
}

TEST(RouterTests, DynamicRouteSingleParamMatch) {
    // GIVEN:
    Router::instance().addRoute("GET", "/user/{id}", [](const HttpRequest& req) {
        EXPECT_EQ(req.params.at("id"), "123");
        return Responses::ok(req, "user", "text/plain");
    });

    HttpRequest req = makeReq("/user/123");

    // WHEN:
    HttpResponse res = Router::instance().route(req);

    // THEN:
    EXPECT_EQ(res.code, StatusCode::OK);
}

TEST(RouterTests, DynamicRouteNotMatchWrongSegments) {
    Router::instance().addRoute("GET", "/user/{id}", [](const HttpRequest& req) {
        return Responses::ok(req, "user", "text/plain");
    });

    HttpRequest req = makeReq("/user/123/extra");
    auto res = Router::instance().route(req);

    EXPECT_EQ(res.code, StatusCode::NotFound);
}

TEST(RouterTests, DynamicRouteMultipleParamsMatch) {
    // GIVEN:
    Router::instance().addRoute("GET", "/post/{year}/{slug}", [](const HttpRequest& req) {
        EXPECT_EQ(req.params.at("year"), "2025");
        EXPECT_EQ(req.params.at("slug"), "article");
        return Responses::ok(req, "post", "text/plain");
    });

    HttpRequest req = makeReq("/post/2025/article");

    // WHEN:
    HttpResponse res = Router::instance().route(req);

    // THEN:
    EXPECT_EQ(res.code, StatusCode::OK);
}

TEST(RouterTests, DynamicRouteMatchDoesNotOverwriteParamsOnFail) {
    // GIVEN:
    Router::instance().addRoute("GET", "/a/{one}", [](const HttpRequest& req) {
        return Responses::ok(req, "a", "text/plain");
    });

    Router::instance().addRoute("GET", "/b/{two}", [](const HttpRequest& req) {
        return Responses::ok(req, "b", "text/plain");
    });

    HttpRequest req = makeReq("/b/xyz");
    
    // WHEN:
    HttpResponse res = Router::instance().route(req);
    
    // THEN:
    EXPECT_EQ(res.code, StatusCode::OK);
    EXPECT_EQ(req.params.at("two"), "xyz");
    EXPECT_EQ(req.params.find("one"), req.params.end()); // Ensure no leftovers
}

TEST(RouterTests, DynamicRouteOrderTest) {
    // GIVEN:
    Router::instance().addRoute("GET", "/user/settings", [](const HttpRequest& req) {
        return Responses::ok(req, "settings", "text/plain");
    });

    Router::instance().addRoute("GET", "/user/{id}", [](const HttpRequest& req) {
        return Responses::ok(req, "dynamic", "text/plain");
    });

    HttpRequest req = makeReq("/user/settings");

    // WHEN:
    HttpResponse res = Router::instance().route(req);

    // THEN:
    EXPECT_TRUE(res.serialize().find("settings") != std::string::npos);
}

TEST(RouterTests, WildcardRouteBasicMatch) {
    namespace fs = std::filesystem;

    // GIVEN:
    fs::path tempDir = fs::temp_directory_path() / "httpserver_static_test";
    fs::create_directories(tempDir);

    // Create a file inside the directory, which our route should serve
    fs::path filePath = tempDir / "file.txt";
    {
        std::ofstream out(filePath);
        ASSERT_TRUE(out.is_open());
        out << "Test file contents";
    }

    Router::instance().addStaticDirectoryRoute("/static", tempDir.string());

    HttpRequest req = makeReq("/static/file.txt");

    // WHEN:
    HttpResponse res = Router::instance().route(req);

    // THEN:
    EXPECT_EQ(res.code, StatusCode::OK);
    EXPECT_EQ(res.body, "Test file contents");

    // CLEANUP
    fs::remove_all(tempDir);
}

TEST(RouterTests, WildcardRouteLongestPrefixWins) {
    namespace fs = std::filesystem;

    // GIVEN:
    fs::path tmp = fs::temp_directory_path() / "httpserver_test_static";
    fs::path baseDir = tmp / "base";
    fs::path imgDir  = tmp / "img";

    fs::create_directories(baseDir);
    fs::create_directories(imgDir);

    // Create a file in imgDir that should be served when the longer prefix matches
    const std::string filename = "pic.png";
    const std::string expectedContent = "PNGDATA";
    {
        std::ofstream ofs(imgDir / filename);
        ofs << expectedContent;
    }

    // Register routes using temp paths
    Router::instance().addStaticDirectoryRoute("/static/", baseDir.string() + "/");
    Router::instance().addStaticDirectoryRoute("/static/images/", imgDir.string() + "/");

    HttpRequest req = makeReq("/static/images/pic.png");

    // WHEN:
    HttpResponse res = Router::instance().route(req);

    // THEN:
    EXPECT_EQ(res.body, expectedContent) << res.body;

    // CLEANUP:
    fs::remove_all(tmp);
}

TEST(RouterTests, WildcardDoesNotMatchShorterPrefix) {
    // GIVEN:
    Router::instance().addStaticDirectoryRoute("/assets/", "files/");

    HttpRequest req = makeReq("/ass");

    // WHEN:
    HttpResponse res = Router::instance().route(req);

    // THEN:
    EXPECT_EQ(res.code, StatusCode::NotFound);
}

TEST(RouterTests, WildcardRouteSanitizesParentTraversal) {
    // GIVEN:
    Router::instance().addStaticDirectoryRoute("/public/", "pub/");

    HttpRequest req = makeReq("/public/../secrets.txt");

    // WHEN:
    HttpResponse res = Router::instance().route(req);

    // THEN:
    EXPECT_EQ(res.code, StatusCode::BadRequest);
}

TEST(RouterTests, WildcardDoesNotOverrideExactMatch) {
    // GIVEN:
    Router::instance().addRoute("GET", "/static", [](const HttpRequest& req) {
        return Responses::ok(req, "exact", "text/plain");
    });

    Router::instance().addStaticDirectoryRoute("/static/", "files/");

    HttpRequest req = makeReq("/static");

    // WHEN:
    HttpResponse res = Router::instance().route(req);

    // THEN:
    EXPECT_TRUE(res.serialize().find("exact") != std::string::npos);
}

TEST(RouterTests, ExactRouteTakesPrecedenceOverDynamicAndWildcard) {
    // GIVEN:
    Router::instance().addRoute("GET", "/page", [](const HttpRequest& req) {
        return Responses::ok(req, "exact", "text/plain");
    });

    Router::instance().addRoute("GET", "/page/{id}", [](const HttpRequest& req) {
        return Responses::ok(req, "dynamic", "text/plain");
    });

    Router::instance().addStaticDirectoryRoute("/page/", "wild/");

    HttpRequest req = makeReq("/page");
    
    // WHEN:
    HttpResponse res = Router::instance().route(req);

    // THEN:
    EXPECT_TRUE(res.serialize().find("exact") != std::string::npos);
}

TEST(RouterTests, DynamicBeatsWildcardWhenBothMatch) {
    // GIVEN:
    Router::instance().addRoute("GET", "/file/{name}", [](const HttpRequest& req) {
        return Responses::ok(req, "dynamic", "text/plain");
    });

    Router::instance().addStaticDirectoryRoute("/file/", "wild/");

    HttpRequest req = makeReq("/file/test");
    
    // WHEN:
    HttpResponse res = Router::instance().route(req);

    // THEN:
    EXPECT_TRUE(res.serialize().find("dynamic") != std::string::npos);
}

TEST(RouterTests, NoRoutesForMethodReturnsNotFound) {
    // GIVEN:
    Router::instance().addRoute("GET", "/hello", [](const HttpRequest& req) {
        return Responses::ok(req, "hi", "text/plain");
    });

    HttpRequest req = makeReq("/hello");
    req.method = "DELETE";

    // WHEN:
    HttpResponse res = Router::instance().route(req);

    // THEN:
    EXPECT_EQ(res.code, StatusCode::NotFound);
}