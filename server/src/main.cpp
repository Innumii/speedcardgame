#include "core/GameServer.hpp"
#include <iostream>
#include <atomic>
#include <csignal>
#include <mutex>
#include <condition_variable>

//allocate null ptr to gameserver
//use for signal handling
static GameServer* serverPtr = nullptr;

//cannot use server.stop() as local var is not in scope
void handleSignal(int) {
    if (serverPtr) {
        std::cout << "\nSignal received. Shutting down...\n";
        serverPtr->stop();
    }
}

//local vars in C++ CANNOT be accessed outside the block they are declared in
int main() {
    //flush buffer
    std::cout.setf(std::ios::unitbuf);
    // std::cout << "Yo\n";
    //allocate on stack
    GameServer server(4000);
    serverPtr = &server;

    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    if(!server.start()) {
        std::cerr << "Failed to start GameServer\n";
        return 1;
    }
    std::cout << "Server Running...\n";
    server.waitForShutdown();

    std::cout << "Server exited cleanly\n";
    return 0;
}