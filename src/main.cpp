#include <httpserver/httpserver.h>
#include <iostream>

int main() {
    using namespace HTTPServer;

    Server server(Port(8080));
    server.installSignalHandlers();

    Router::instance().addRoute("GET", "/", [](const HttpRequest& req) {
        return Responses::ok(req, "Welcome to the home page!");
    });

    Router::instance().addRoute("GET", "/about", [](const HttpRequest& req) {
        return Responses::ok(req, "About page");
    });

    Router::instance().addRoute("POST", "/submit", [](const HttpRequest& req) {
        return Responses::ok(req, "Form submitted!");
    });

    server.start();
    std::cout << "Server exited cleanly" << std::endl;
}