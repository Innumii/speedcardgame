#include "core/Server.hpp"
#include <iostream>

int main() {
    TcpServer server(4000);

    if (!server.start()) {
        std::cerr << "Failed to start server\n";
        return 1;
    }

    std::cout << "Server running. Press Enter to stop...\n";
    std::cin.get();

    server.stop();
    return 0;
}