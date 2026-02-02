#include "core/Server.hpp"
#include <iostream>

int main() {
    try {
        Server server(5555);
        server.run();
    }
    catch (const std::exception& e) {
        std::cerr << "Server crashed: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}