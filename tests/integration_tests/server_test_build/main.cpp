#include <httpserver/httpserver.h>
#include <iostream>
#include <cstdlib>

using namespace HTTPServer;

static std::string getEnvStr(const char* envVar, const std::string& fallback) {
    if (const char* v = std::getenv(envVar))
        return v;
    return fallback;
}

static int getEnvInt(const char* envVar, const int fallback) {
    const char* p = getenv(envVar);
    if (!p) return fallback;
    return std::stoi(p);
}

int main() {
    std::string static_dir = getEnvStr("TEST_STATIC_DIR", "public/");
    const char* cert = getenv("TEST_HTTPS_CERT");
    const char* key  = getenv("TEST_HTTPS_KEY");
    int enable_https = getEnvInt("TEST_ENABLE_HTTPS", 0);

    Port http_port = enable_https ? Port(8443) : Port(8080);
    Server server(http_port);
    server.installSignalHandlers();

    if (enable_https && cert && key) {
        server.enableHttps(cert, key);
    }

    // Basic route case
    Router::instance().addRoute("GET", "/", [](const HttpRequest& req) {
        return Responses::ok(req, "OK");
    });

    // Simple route which expects a single parameter 'input'
    Router::instance().addRoute("GET", "/param", [](const HttpRequest& req) {
        auto it = req.params.find("input");
        if (it != req.params.end()) {
            return Responses::ok(req, "Parameter: " + it->second);
        }

        return Responses::notFound(req);
    });

    // Simple dynamic route
    Router::instance().addRoute("GET", "/dynamic/{uuid}", [](const HttpRequest& req) {
        auto it = req.params.find("uuid");
        if (it != req.params.end()) {
            return Responses::ok(req, "GET [dynamic] request recieved: " + it -> second);
        }
        return Responses::notFound(req);
    });

    // Static directory route
    Router::instance().addStaticDirectoryRoute("/static", static_dir);

    server.start();
    std::cout << "Server exited cleanly" << std::endl;
    return 0;
}