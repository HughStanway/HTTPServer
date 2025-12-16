#include <httpserver/httpserver.h>
#include <iostream>

int main() {
    using namespace HTTPServer;

    Server server(Port(443));
    server.installSignalHandlers();
    server.enableHttps(".env/cert.pem", ".env/key.pem");
    server.enableHttpRedirection(Port(80));

    Router::instance().addRoute("GET", "/", [](const HttpRequest& req) {
        return Responses::file(req, "public/index.html");
    });

    Router::instance().addRoute("GET", "/about", [](const HttpRequest& req) {
        return Responses::file(req, "public/about.html");
    });

    Router::instance().addRoute("GET", "/contact", [](const HttpRequest& req) {
        return Responses::file(req, "public/contact.html");
    });

    Router::instance().addRoute("GET", "/usr/{uuid}", [](const HttpRequest& req) {
        auto it = req.params.find("uuid");
        if (it != req.params.end()) {
            std::cout << "Recieved usr request: " << it->second << std::endl;
            return Responses::ok(req, "GET [usr] request recieved");
        }
        return Responses::notFound(req);
    });

    Router::instance().addRoute("GET", "/add", [](const HttpRequest& req) {
        auto itr = req.params.find("email");
        if (itr != req.params.end()) {
            std::cout << "Received email param: " << itr->second << std::endl;
            return Responses::ok(req, "Email received");
        }

        return Responses::notFound(req);
    });

    Router::instance().addStaticDirectoryRoute("/static", "public/");

    server.start();
    std::cout << "Server exited cleanly" << std::endl;
}