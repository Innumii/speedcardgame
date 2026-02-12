#include "core/Server.hpp"
#include <iostream>
#include <atomic>
#include <csignal>
#include <mutex>
#include <condition_variable>

std::atomic<bool> running(true);
std::mutex mtx;
std::condition_variable cv;

void handleSignal(int) {
    running = false;
    cv.notify_all(); // wake main thread
}

int main() {
    std::cout.setf(std::ios::unitbuf); // automatic flush after every output

    // Handle Ctrl+C and Docker stop
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    TcpServer server(4000);

    if (!server.start()) {
        std::cerr << "Failed to start server\n";
        return 1;
    }

    std::cout << "Server running...\n";

    // Industry-standard: main thread blocks, zero CPU usage
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [](){ return !running.load(); });

    std::cout << "Shutting down server...\n";
    server.stop();

    return 0;
}
