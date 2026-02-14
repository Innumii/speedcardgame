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

        // Register callback for new clients
        tcpServer->onClientConnected = [this](std::shared_ptr<PlayerConnection> player) {
            if (matchmaker) {
                matchmaker->enqueuePlayer(player);
            }
        };

        if (!tcpServer->start()) {
            std::cerr << "Failed to start TcpServer\n";
            tcpServer.reset(); // cleanup
            return false;
        }

        std::cout << "TcpServer started on port " << port << "\n";

        // -------------------------------
        // 2️⃣ Start Matchmaker
        // -------------------------------
        matchmaker = std::make_unique<Matchmaker>();
        if (!matchmaker->start()) {
            std::cerr << "Failed to start Matchmaker\n";
            tcpServer->stop(); // rollback TcpServer
            tcpServer.reset();
            matchmaker.reset();
            return false;
        }

        std::cout << "Matchmaker started\n";

        running = true;
        return true;

    } catch (const std::exception& ex) {
        // Catch unexpected exceptions and rollback everything
        std::cerr << "Exception during GameServer startup: " << ex.what() << "\n";
        if (matchmaker) matchmaker->stop();
        if (tcpServer) tcpServer->stop();
        tcpServer.reset();
        matchmaker.reset();
        running = false;
        return false;
    } catch (...) {
        std::cerr << "Unknown exception during GameServer startup\n";
        if (matchmaker) matchmaker->stop();
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

    // Stop subsystems in reverse order
    if (matchmaker) {
        matchmaker->stop();
        matchmaker.reset();
    }

    if (tcpServer) {
        tcpServer->stop();
        tcpServer.reset();
    }

    // Wake main thread
    shutdownCv.notify_all();
}

void GameServer::waitForShutdown() {
    std::unique_lock<std::mutex> lock(shutdownMutex);
    shutdownCv.wait(lock, [this]() { return !running.load(); });
}
