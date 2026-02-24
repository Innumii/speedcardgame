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
#include <openssl/ssl.h>
#include <openssl/err.h>

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

    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    sslCtx = SSL_CTX_new(TLS_server_method());
    if (!sslCtx) { std::cerr << "[TcpServer] Failed to create SSL context\n"; return false; }

    // Load server certificate and private key
    if (SSL_CTX_use_certificate_file(sslCtx, "/certs/server.crt", SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        return false;
    }

    if (SSL_CTX_use_PrivateKey_file(sslCtx, "/certs/server.key", SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        return false;
    }

    // Verify that private key matches the certificate
    if (!SSL_CTX_check_private_key(sslCtx)) {
        std::cerr << "[TcpServer] Private key does not match the certificate public key\n";
        return false;
    }

    std::cout << "[TcpServer] TLS certificate and key loaded successfully\n";


    running = true;

    // 4️⃣ Start accept/disconnect threads
    acceptThread = std::thread(&TcpServer::acceptClients, this);
    disconnectThread = std::thread(&TcpServer::disconnectLoop, this);

    std::cout << "[TcpServer] Server listening on port " << listenPort << "\n";

    

    return true;
}

void TcpServer::stop() {
    std::cout << "[TcpServer] Stopping...\n";
    if (!running) return;

    running = false;

    if (listenSocket >= 0) {
        shutdown(listenSocket, SHUT_RDWR);
        close(listenSocket);
        listenSocket = -1;
    }

    if (acceptThread.joinable()) {
        acceptThread.join();
    }

    {
        std::lock_guard<std::mutex> lock(disconnectMutex);
        disconnectRunning = false;
    }
    disconnectCv.notify_one();
    if (disconnectThread.joinable()) disconnectThread.join();

    // Stop all remaining clients
    std::cout << "[TcpServer] Removing all players...\n";
    std::vector<std::shared_ptr<PlayerConnection>> copy;
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        copy = clients;
        clients.clear();
    }
    for (auto& player : copy) player->stop();
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
        auto player = std::make_shared<PlayerConnection>(clientSock, sslCtx);

        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            clients.push_back(player);   // <-- REAL OWNER
        }

        // Fire callback (non-blocking in accept thread)
        if (onClientConnected) {
            try {
                onClientConnected(player);
            } catch (const std::exception& ex) {
                std::cerr << "[TcpServer] Exception in onClientConnected callback: " << ex.what() << "\n";
            } catch (...) {
                std::cerr << "[TcpServer] Unknown exception in onClientConnected callback\n";
            }
        }

    }
}

//Disconnect Handling below
void TcpServer::removeClient(const std::shared_ptr<PlayerConnection>& player){
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        clients.erase(std::remove(clients.begin(), clients.end(), player), clients.end());
    }

    player->stop();
}

void TcpServer::enqueueDisconnect(const std::shared_ptr<PlayerConnection>& player) {
    {
        std::lock_guard<std::mutex> lock(disconnectMutex);
        disconnectQueue.push(player);
    }
    disconnectCv.notify_one();  // wake up dc thread
}


void TcpServer::disconnectLoop() {
    disconnectRunning = true;
    while (disconnectRunning) {
        std::shared_ptr<PlayerConnection> player;

        {
            std::unique_lock<std::mutex> lock(disconnectMutex);
            disconnectCv.wait(lock, [this]() {
                return !disconnectQueue.empty() || !disconnectRunning;
            });

            if (!disconnectRunning) break;

            player = disconnectQueue.front();
            disconnectQueue.pop();
        }

        if (player) {
            removeClient(player);  // stops thread, closes socket, removes from container
        }
    }
}