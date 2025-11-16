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

    Router::instance().addRoute("GET", "/css/style.css", [](const HttpRequest& req) {
        return Responses::file(req, "public/css/style.css");
    });

    Router::instance().addRoute("GET", "/js/main.js", [](const HttpRequest& req) {
        return Responses::file(req, "public/js/main.js");
    });

    server.start();
    std::cout << "Server exited cleanly" << std::endl;
}