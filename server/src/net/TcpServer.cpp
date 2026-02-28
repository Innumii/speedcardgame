#include "net/TcpServer.hpp"
#include "net/PlayerConnection.hpp" // wrapper for client socket
#include <iostream>
#include <thread>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>

TcpServer::TcpServer(int port)
    : listenPort(port), listenSocket(-1), running(false)
{
}

TcpServer::~TcpServer() {
    stop();
}

bool TcpServer::start() {
    if (running) return false;

    // 1️⃣ Create socket
    listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket < 0) {
        perror("socket");
        return false;
    }

    // Allow quick restarts
    int opt = 1;
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 2️⃣ Bind to port
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(listenPort);

    if (bind(listenSocket, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(listenSocket);
        listenSocket = -1;
        return false;
    }

    // 3️⃣ Listen for connections
    if (listen(listenSocket, SOMAXCONN) < 0) {
        perror("listen");
        close(listenSocket);
        listenSocket = -1;
        return false;
    }

    running = true;

    // 4️⃣ Start accept thread
    acceptThread = std::thread(&TcpServer::acceptClients, this);
    std::cout << "Server listening on port " << listenPort << "\n";

    return true;
}

void TcpServer::stop() {
    if (!running) return;

    running = false;

    // Close listening socket to unblock accept()
    if (listenSocket >= 0) {
        close(listenSocket);
        listenSocket = -1;
    }

    // Join accept thread
    if (acceptThread.joinable()) {
        acceptThread.join();
    }

    // Close all client sockets
    std::lock_guard<std::mutex> lock(clientsMutex);
    for (int sock : clientSockets) {
        close(sock);
    }
    clientSockets.clear();
}

void TcpServer::acceptClients() {
    while (running) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);

        int clientSock = accept(listenSocket, (sockaddr*)&clientAddr, &clientLen);

        if (clientSock < 0) {
            if (running) perror("accept");
            continue;
        }

        // Wrap raw socket in PlayerConnection (future-proof)
        auto player = std::make_shared<PlayerConnection>(clientSock);

        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            clientSockets.push_back(clientSock);
        }


        // Fire callback (non-blocking in accept thread)
        if (onClientConnected) {
            try {
                onClientConnected(player);
            } catch (const std::exception& ex) {
                std::cerr << "Exception in onClientConnected callback: " << ex.what() << "\n";
            } catch (...) {
                std::cerr << "Unknown exception in onClientConnected callback\n";
            }
        }
    }
}
