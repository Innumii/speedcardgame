//Socket Handling
#include "net/TcpServer.hpp"
#include <stdexcept>
#include <iostream>
#include <thread>
#include <chrono>

//socket libraries
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h> //sockaddr_in
#include <unistd.h> //close()
#include <arpa/inet.h>

//memset()
#include <cstring>

//initialise listeners BEFORE construction of object
//socket should be in invalid state first (-1)
TcpServer::TcpServer(int port):listenPort(port),listenSocket(-1),running(false) {

}

TcpServer::~TcpServer() {
    stop();
}

bool TcpServer::start() {
    if (running) return false;

    listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket < 0) {
        perror("socket");
        return false;
    }
    //allow fast restarts
    int opt = 1;
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    //bind socket to port
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(listenPort);

    if (bind(listenSocket, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(listenSocket);
        return false;
    }
    
    //listen for connections
    if (listen(listenSocket, SOMAXCONN) < 0) {
        perror("listen");
        close(listenSocket);
        return false;
    }

    running = true;
    acceptThread = std::thread(&TcpServer::acceptClients, this);
    std::cout << "Server listening on port " << listenPort << "\n";
    return true;
}

void TcpServer::stop() {
    if (!running) return;
    running = false;

    //stop accept loop, close socket
    if (listenSocket >= 0) {
        close(listenSocket);
        listenSocket = -1;
    }
    //join accept thread
    if (acceptThread.joinable()) {
        acceptThread.join();
    }
    //close all client sockets
    std::lock_guard<std::mutex> lock(clientsMutex);
    for (int client : clientSockets) {
        close(client);
    }
    clientSockets.clear();
}

void TcpServer::acceptClients() {
    while (running) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);

        int clientSocket = accept(
            listenSocket, 
            (sockaddr*)&clientAddr,
            &clientLen
        );
        
        if (clientSocket < 0) {
            if(running) perror("accept");
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            clientSockets.push_back(clientSocket);
        }

        std::cout << "Client connected from "
                  << inet_ntoa(clientAddr.sin_addr)
                  << "\n";

        // TODO:
        // - If >= 2 clients are waiting
        // - Pop two sockets
        // - Create MatchSession(clientA, clientB)
    }
}

// void TcpServer::handleClient(int clientSocket) {

// }