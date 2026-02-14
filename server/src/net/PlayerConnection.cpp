#include "net/PlayerConnection.hpp"
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <sys/socket.h>

PlayerConnection::PlayerConnection(int socket):clientSocket(socket), running(false) {

}

PlayerConnection::~PlayerConnection() {
    stop();
}

bool PlayerConnection::start() {
    if (running) return false;
    
    try {
        readThread = std::thread(&PlayerConnection::readLoop, this);
    } catch (const std::system_error& e) {
        std::cerr << "Failed to start read Thread " << e.what() << "\n";
        return false;
    }

    running = true;
    return true;
}

void PlayerConnection::stop() {
    if (!running) return;
    if (clientSocket >= 0) {
        close(clientSocket);
        clientSocket = -1;
    }

    if (readThread.joinable()) {
        readThread.join();
    }
}

int PlayerConnection::getSocket() const {
    return clientSocket;
}

void PlayerConnection::readLoop() {
    constexpr size_t bufferSize = 1024;
    char buffer[bufferSize];

    while (running) {
        ssize_t bytesRead = recv(clientSocket, buffer, bufferSize, 0);
        if (bytesRead <= 0) {
            running = false;  // client disconnected or error
            break;
        }

        std::vector<char> message(buffer, buffer + bytesRead);

        // Fire the callback
        if (onMessageReceived) {
            try {
                onMessageReceived(message);
            } catch (const std::exception& ex) {
                std::cerr << "Exception in onMessageReceived: " << ex.what() << "\n";
            } catch (...) {
                std::cerr << "Unknown exception in onMessageReceived\n";
            }
        }
    }
}