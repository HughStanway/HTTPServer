#include "main.h"

#include <iostream>
#include <csignal>

#include "server.h"

HTTPServer::Server* g_server = nullptr;

void signal_handler(int signum) {
    std::cout << "\nSIGINT received, shutting down..." << std::endl;
    if (g_server) g_server->stop();
}

int main() {
    HTTPServer::Server server(HTTPServer::Port(8080));
    g_server = &server;

    std::signal(SIGINT, signal_handler);

    server.start();

    std::cout << "Server exited cleanly." << std::endl;
    return 0;
}
