#ifndef SERVER_H
#define SERVER_H

#include "port.h"

namespace HTTPServer {

class Server {
    public: 
        explicit Server(Port port);
        Server(const Server&) = delete;
        Server& operator=(const Server&) = delete;

        const Port& port() const;

        void start();
        void stop();

    private:
        const Port d_port;
        int server_fd;

        void handle_client(int client_fd);
 };

} // namespace HTTPServer

#endif