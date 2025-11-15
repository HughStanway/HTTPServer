#include "main.h"

#include <iostream>

#include "server.h"

int main() {
	HTTPServer::Server server(HTTPServer::Port(8080));
	server.start();
	server.stop();

	return 0;
}
