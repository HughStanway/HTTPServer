#include "httpserver/server.h"

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <csignal>
#include <iostream>
#include <thread>

#include "httpserver/http_object.h"
#include "httpserver/http_parser.h"
#include "httpserver/http_response.h"
#include "httpserver/http_response_builder.h"
#include "httpserver/logger.h"
#include "httpserver/port.h"
#include "httpserver/utils.h"
#include "httpserver/router.h"

namespace {

HTTPServer::Server* g_activeServer = nullptr;

void sig_handler(int) {
    LOG_INFO("SIGINT or SIGTERM received, shutting down...");
    if (g_activeServer) {
        g_activeServer->stop();
    }
}

} // namespace global

namespace HTTPServer {

Server::Server(Port port) : d_port(port), server_fd(-1) {}

const Port &Server::port() const { return d_port; }

void Server::stop() {
    if (!d_running)
        return;

    d_running = false;
    LOG_INFO("Stopping server on port " + d_port.toString() + " ...");

    if (!(server_fd < 0)) {
        close(server_fd);
    }

    // Join all client threads
    for (auto &t : client_threads) {
        if (t.joinable())
            t.join();
    }
    client_threads.clear();
    LOG_INFO("All client threads finished.");
}

void Server::installSignalHandlers() {
    g_activeServer = this;
    std::signal(SIGINT, sig_handler);
    std::signal(SIGTERM, sig_handler);
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
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = d_port.toNetwork();

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
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
        int client_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (client_fd < 0) {
            if (!d_running)
                break; // socket closed, exit loop
            LOG_ERROR_ERRNO("accept failed");
            continue;
        }

        struct timeval timeout;
        timeout.tv_sec = 5;   // idle timeout seconds
        timeout.tv_usec = 0;
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

        LOG_INFO("Accepted client [" + std::to_string(client_fd) + "]");
        client_threads.emplace_back(&Server::handle_client, this, client_fd);
    }
    LOG_INFO("Server main loop exited.");
}

void Server::handle_client(int client_fd) {
    LOG_INFO("Client [" + std::to_string(client_fd) + "] connected");

    const int MAX_KEEPALIVE_REQUESTS = 100;
    bool keepAlive = true;
    int requests_handled = 0;
    while (keepAlive && requests_handled < MAX_KEEPALIVE_REQUESTS) {
        char buffer[4096];
        int bytes = recv(client_fd, buffer, sizeof(buffer), 0);
        if (bytes == 0) {
            LOG_INFO("Client closed connection");
            break;
        }

        if (bytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                LOG_INFO("Idle timeout reached, closing");
            } else {
                LOG_INFO("recv error");
            }
            break;
        }

        std::string raw(buffer, bytes);

        HttpRequest request;
        ParseError err = HttpParser::parse(raw, request);

        HttpResponse response;
        if (err != ParseError::NONE) {
            LOG_ERROR("Bad HTTP request");
            response = Responses::badRequest();
            keepAlive = false;
        } else {
            LOG_INFO("Parsed request from client [" + std::to_string(client_fd) + "]: " + request.method + " " + request.path);
            //response = Responses::ok(request, "Hello World!");
            response = Router::instance().route(request);
            keepAlive = requestWantsKeepAlive(request);
        }

        std::string payload = response.serialize();
        send(client_fd, payload.c_str(), payload.size(), 0);

        requests_handled++;

        if (!keepAlive)
            break;
    }

    close(client_fd);
    LOG_INFO("Client [" + std::to_string(client_fd) + "] disconnected");
}

} // namespace HTTPServer