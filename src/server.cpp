#include "server.h"

#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <thread>

#include "port.h"
#include "logger.h"

namespace HTTPServer {

Server::Server(Port port) : d_port(port), server_fd(-1) {}

const Port& Server::port() const {
    return d_port;
}

void Server::stop() {
    if (!d_running) return;

    d_running = false;
    LOG_INFO("Stopping server on port " + d_port.toString() + " ...");

    if (!(server_fd < 0)) {
        close(server_fd);
    }

    // Join all client threads
    for (auto& t : client_threads) {
        if (t.joinable()) t.join();
    }
    client_threads.clear();
    LOG_INFO("All client threads finished.");
}

void Server::start() {
    LOG_INFO("Starting server on port " + d_port.toString() + " ...");
    // 1. Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        LOG_ERROR_ERRNO("socket failed");
        return;
    }

    // Allow quick rebinding to the same port after restart
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 2. Bind to port
    sockaddr_in address{};
    address.sin_family      = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port        = d_port.toNetwork();

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        LOG_ERROR_ERRNO("bind failed");
        close(server_fd);
        return;
    }

    // 3. Start listening
    if (listen(server_fd, 10) < 0) {
        LOG_ERROR_ERRNO("listen failed");
        close(server_fd);
        return;
    }

    d_running = true;
    LOG_INFO("Server running on port " + d_port.toString() + " ...");
    while (d_running) {
        socklen_t addrlen = sizeof(address);
        int client_fd = accept(server_fd, (struct sockaddr*)&address, &addrlen);
        if (client_fd < 0) {
            if (!d_running) break; // socket closed, exit loop
            LOG_ERROR_ERRNO("accept failed");
            continue;
        }

        LOG_INFO("Accepted client [" + std::to_string(client_fd) + "]");
        client_threads.emplace_back(&Server::handle_client, this, client_fd);
    }
    LOG_INFO("Server main loop exited.");
}

void Server::handle_client(int client_fd) {
    LOG_INFO("Client [" + std::to_string(client_fd) + "] connected");

    char buffer[4096] = {0};
    int bytes = recv(client_fd, buffer, sizeof(buffer), 0);
    if (bytes < 0) {
        LOG_ERROR_ERRNO("recv failed");
    } else {
        LOG_INFO("Received (" + std::to_string(bytes) + " bytes) from client [" + std::to_string(client_fd) + "]:");
        std::cout << buffer;
    }

    const char* msg = "Hello from your multithreaded TCP server!\n";
    send(client_fd, msg, strlen(msg), 0);

    close(client_fd);
    LOG_INFO("Client [" + std::to_string(client_fd) + "] disconnected");
}

} // namespace HTTPServer