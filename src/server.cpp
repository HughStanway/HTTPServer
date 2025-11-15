#include "server.h"

#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <thread>

#include "port.h"

namespace HTTPServer {

Server::Server(Port port) : d_port(port), server_fd(-1) {}

const Port& Server::port() const {
    return d_port;
}

void Server::stop() {
    std::cout << "Stopping server on port " << d_port.value() <<" ..." << std::endl;
    if (!(server_fd < 0)) {
        close(server_fd);
    }
}

void Server::start() {
    std::cout << "Starting server on port " << d_port.value() << " ..." << std::endl;

    // 1. Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
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
        perror("bind");
        close(server_fd);
        return;
    }

    // 3. Start listening
    if (listen(server_fd, 10) < 0) {
        perror("listen");
        close(server_fd);
        return;
    }

    std::cout << "Server running on port " << d_port.value() << " ..." << std::endl;
    while (true) {
        socklen_t addrlen = sizeof(address);
        int client_fd = accept(server_fd, (struct sockaddr*)&address, &addrlen);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }
        std::thread(&Server::handle_client, this, client_fd).detach();
    }
}

void Server::handle_client(int client_fd) {
    std::cout << "Client [" << client_fd << "] connected" << std::endl;

    char buffer[4096] = {0};
    int bytes = recv(client_fd, buffer, sizeof(buffer), 0);
    if (bytes < 0) {
        perror("recv");
    } else {
        std::cout << "Received (" << bytes << " bytes):\n";
        std::cout << buffer << "\n";
    }

    const char* msg = "Hello from your multithreaded TCP server!\n";
    send(client_fd, msg, strlen(msg), 0);

    close(client_fd);
    std::cout << "Client [" << client_fd << "] disconnected" << std::endl;
}

} // namespace HTTPServer