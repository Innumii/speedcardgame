#include "core/GameServer.hpp"
#include <iostream>
#include <atomic>
#include <csignal>
#include <mutex>
#include <condition_variable>
#include <execinfo.h>  // ← add this
#include <unistd.h>    // ← add this

static GameServer* serverPtr = nullptr;

void handleSignal(int) {
    if (serverPtr) {
        std::cout << "\nSignal received. Shutting down...\n";
        serverPtr->stop();
    }
}

void crashHandler(int sig) {
    void* array[32];
    int size = backtrace(array, 32);
    write(STDERR_FILENO, "=== CRASH ===\n", 14);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    _exit(1);
}

int main() {
    setvbuf(stderr, nullptr, _IONBF, 0);  // ← add
    setvbuf(stdout, nullptr, _IONBF, 0);  // ← add

    signal(SIGSEGV, crashHandler);  // ← add
    signal(SIGABRT, crashHandler);  // ← add

    std::set_terminate([]{
        std::cerr << "💥 std::terminate called\n";
        std::abort();
    });

    std::cout.setf(std::ios::unitbuf);
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