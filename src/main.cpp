#include "main.h"

#include <csignal>
#include <iostream>

#include "logger.h"
#include "server.h"

HTTPServer::Server* g_server = nullptr;

void signal_handler(int signum) {
    LOG_INFO("SIGINT received, shutting down...");
    if (g_server) g_server->stop();
}

int main() {
    HTTPServer::Server server(HTTPServer::Port(8080));
    g_server = &server;

    std::signal(SIGINT, signal_handler);

    server.start();

    LOG_INFO("Server exited cleanly.");
    return 0;
}
