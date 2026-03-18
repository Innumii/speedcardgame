#include "net/PlayerConnection.hpp"
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <sys/socket.h>

PlayerConnection::PlayerConnection(int socket, SSL_CTX* ctx)
    : clientSocket(socket), sslCtx(ctx), running(false)
{
    ssl = SSL_new(sslCtx);
    SSL_set_fd(ssl, clientSocket);

    if (SSL_accept(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        ssl = nullptr;
        std::cerr << "[PlayerConnection] TLS handshake failed\n";
    }
}

PlayerConnection::~PlayerConnection() {
    stop();
    std::cout << "[PlayerConnection] Destroyed: " << username << "\n";
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
    if (running || !ssl) return false;
    running = true;
    try {
        auto self = shared_from_this();
        readThread = std::thread([self]() {
            self->readLoop();
        });
    } catch (const std::system_error& e) {
        std::cerr << "[PlayerConnection] Failed to start read Thread " << e.what() << "\n";
        running = false;
    }

    return running;
}

void PlayerConnection::stop() {
    std::lock_guard<std::mutex> lock(stopMutex);
    if (stopped) return;
    stopped = true;

    running = false;
    
    {
        std::lock_guard<std::mutex> lock(writeMutex); // wait for writes to finish
        if (ssl) {
            SSL_shutdown(ssl);
            SSL_free(ssl);
            ssl = nullptr;
        }
    }

    if (clientSocket >= 0) {
        shutdown(clientSocket, SHUT_RDWR);
        close(clientSocket);
        clientSocket = -1;
    }

    if (readThread.joinable() && std::this_thread::get_id() != readThread.get_id())
        readThread.join();
}

int PlayerConnection::getSocket() const {
    return clientSocket;
}

//actions
bool PlayerConnection::send(const std::string& msg) {
    if (!running || !ssl) return false;

    std::lock_guard<std::mutex> lock(writeMutex); // serialize writes
    int n = SSL_write(ssl, msg.c_str(), msg.size());
    return n == static_cast<int>(msg.size());
}

// bool PlayerConnection::pollMessage(std::string& outMsg) {
//     std::lock_guard<std::mutex> lock(queueMutex);
//     if (messageQueue.empty()) return false;
//     outMsg = messageQueue.front();
//     messageQueue.pop();
//     return true;
// }

bool PlayerConnection::isAlive() const {
    return running.load();
}

//blocking btw
void PlayerConnection::readLoop() {
    constexpr size_t bufferSize = 1024;
    char buffer[bufferSize];

    while (running && ssl) {
        int bytes = SSL_read(ssl, buffer, sizeof(buffer));
        if (bytes <= 0) {
            int err = SSL_get_error(ssl, bytes);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
                continue;

            break; // disconnected or fatal SSL error
        }

        std::vector<char> msg(buffer, buffer + bytes);

        try {
            dispatchMessage(msg);
        } catch (const std::exception& e) {
            std::cerr << "[PlayerConnection] Exception in dispatchMessage: " << e.what() << "\n";
        } catch (...) {
            std::cerr << "[PlayerConnection] Unknown exception in dispatchMessage\n";
        }

    }

    // fire disconnect once
    if (onDisconnected) onDisconnected();
}

void PlayerConnection::dispatchMessage(const std::vector<char>& rawMsg) {
    std::string msg(rawMsg.begin(), rawMsg.end());
    if (auto it = stateHandlers.find(state); it != stateHandlers.end()) {
        it->second(shared_from_this(), msg);  // call the callback for the current state
    } else {
        std::cerr << "[PlayerConnection] No handler for state " << int(state) << "\n";
    }
}

void PlayerConnection::setMessageHandler(ConnectionState state, MsgCallback cb) {
        stateHandlers[state] = std::move(cb);
}