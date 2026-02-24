#include "net/PlayerConnection.hpp"
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <sys/socket.h>

PlayerConnection::PlayerConnection(int socket):clientSocket(socket), running(false) {

}

PlayerConnection::~PlayerConnection() {
    std::cout << "[DEBUG] PlayerConnection destroyed for: " << username << "\n";
    // stop();
}

void PlayerConnection::setPlayerInfo(int id, const std::string& name) {
    playerId = id;
    username = name;
}

int PlayerConnection::getPlayerId() const {
    return playerId;
}
const std::string& PlayerConnection::getUsername() const{
    return username;
}

bool PlayerConnection::start() {
    if (running) return false;
    running = true;
    try {
        auto self = shared_from_this();
        readThread = std::thread([self]() {
            self->readLoop();
        });
    } catch (const std::system_error& e) {
        std::cerr << "Failed to start read Thread " << e.what() << "\n";
        running = false;
    }

    return running;
}

void PlayerConnection::stop() {
    std::cout << "[STOP ENTER] " << username
          << " running=" << running
          << " hasCallback=" << (onDisconnected ? "YES" : "NO")
          << "\n";
    auto self = shared_from_this(); // keep alive

    if (onDisconnected) {
        std::cout << "onDisconnected triggering\n";
        auto cb = std::move(onDisconnected);
        onDisconnected = nullptr;

        try {
            std::cout << "Disconnecting " << username << "\n";
            cb();
        } catch (...) {
            std::cerr << "Exception in onDisconnected\n";
        }
    }

    if (clientSocket >= 0) {
        std::cout << "Shutting Down Sockets...\n";
        shutdown(clientSocket, SHUT_RDWR);
        close(clientSocket);
        clientSocket = -1;
    }

    //2nd stop() calls this
    if (readThread.joinable() &&
        std::this_thread::get_id() != readThread.get_id()) {
        std::cout << "Killing PlayerConnection Threads...\n";
        readThread.join();
    }
}

int PlayerConnection::getSocket() const {
    return clientSocket;
}

//actions
bool PlayerConnection::send(const std::string& msg) {
    if (!running) return false;
    ssize_t n = ::send(clientSocket, msg.c_str(), msg.size(), 0);
    return n == static_cast<ssize_t>(msg.size());
}

bool PlayerConnection::pollMessage(std::string& outMsg) {
    std::lock_guard<std::mutex> lock(queueMutex);
    if (messageQueue.empty()) return false;
    outMsg = messageQueue.front();
    messageQueue.pop();
    return true;
}

bool PlayerConnection::isAlive() const {
    return running.load();
}

//blocking btw
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

    stop();
}