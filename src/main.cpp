#include "main.h"

#include <csignal>
#include <iostream>

#include "logger.h"
#include "server.h"
#include "router.h"
#include "http_response_builder.h"

HTTPServer::Server *g_server = nullptr;

void signal_handler(int signum) {
    LOG_INFO("SIGINT received, shutting down...");
    if (g_server)
        g_server->stop();
}

int main() {
    HTTPServer::Server server(HTTPServer::Port(8080));
    g_server = &server;

    HTTPServer::Router::instance().addRoute("GET", "/", [](const HTTPServer::HttpRequest& req) {
    return HTTPServer::Responses::ok(req, "Welcome to the home page!");
    });

    HTTPServer::Router::instance().addRoute("GET", "/about", [](const HTTPServer::HttpRequest& req) {
        return HTTPServer::Responses::ok(req, "About page");
    });

    HTTPServer::Router::instance().addRoute("POST", "/submit", [](const HTTPServer::HttpRequest& req) {
        return HTTPServer::Responses::ok(req, "Form submitted!");
    });

    std::signal(SIGINT, signal_handler);

    server.start();

    LOG_INFO("Server exited cleanly.");
    return 0;
}
