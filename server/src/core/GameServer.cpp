#include "core/GameServer.hpp"
#include "net/TcpServer.hpp"
#include "core/Matchmaker.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>

GameServer::GameServer(int port)
    : port(port), running(false)
{
}

bool GameServer::start() {
    running = false;

    try {
        // -------------------------------
        // 1️⃣ Start TcpServer
        // -------------------------------
        tcpServer = std::make_unique<TcpServer>(port);

        // Initialize Matchmaker (passive)
        matchmaker = std::make_unique<Matchmaker>();

        // Register callback for new clients
        tcpServer->onClientConnected = [this](std::shared_ptr<PlayerConnection> player) {
            if (matchmaker) {
                matchmaker->enqueuePlayer(player);
            }
        };

        if (!tcpServer->start()) {
            std::cerr << "Failed to start TcpServer\n";
            tcpServer.reset(); // cleanup
            matchmaker.reset();
            return false;
        }

        std::cout << "TcpServer started on port " << port << "\n";
        std::cout << "Matchmaker initialized\n";

        running = true;
        return true;

    } catch (const std::exception& ex) {
        std::cerr << "Exception during GameServer startup: " << ex.what() << "\n";
        if (tcpServer) tcpServer->stop();
        tcpServer.reset();
        matchmaker.reset();
        running = false;
        return false;
    } catch (...) {
        std::cerr << "Unknown exception during GameServer startup\n";
        if (tcpServer) tcpServer->stop();
        tcpServer.reset();
        matchmaker.reset();
        running = false;
        return false;
    }
}

void GameServer::stop() {
    if (!running) return;

    running = false;
    std::cout << "Stopping GameServer...\n";

    // Stop TcpServer first
    if (tcpServer) {
        tcpServer->stop();
        tcpServer.reset();
    }

    // Matchmaker is passive; just destroy
    matchmaker.reset();

    // Wake main thread if blocked
    shutdownCv.notify_all();
}

void GameServer::waitForShutdown() {
    std::unique_lock<std::mutex> lock(shutdownMutex);
    shutdownCv.wait(lock, [this]() { return !running.load(); });
}
