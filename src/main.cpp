#include <httpserver/httpserver.h>
#include <iostream>

int main() {
    using namespace HTTPServer;

    Server server(Port(8080));
    server.installSignalHandlers();

    Router::instance().addRoute("GET", "/", [](const HttpRequest& req) {
        return Responses::file(req, "public/index.html");
    });

    Router::instance().addRoute("GET", "/about", [](const HttpRequest& req) {
        return Responses::file(req, "public/about.html");
    });

    Router::instance().addRoute("GET", "/contact", [](const HttpRequest& req) {
        return Responses::file(req, "public/contact.html");
    });

    Router::instance().addStaticDirectoryRoute("/static", "public/");

    server.start();
    std::cout << "Server exited cleanly" << std::endl;
}