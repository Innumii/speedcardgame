#include "core/NetworkClient.hpp"
#include <iostream>
#include <cstring>
#include <cerrno>

NetworkClient::NetworkClient(SocketMode m) : socketFd(-1), connected(false), mode(m) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
    }
#endif
}

NetworkClient::~NetworkClient() {
    disconnect();
#ifdef _WIN32
    WSACleanup();
#endif
}

bool NetworkClient::connectTo(const std::string& ip, int port) {
    if (connected) return false;

    socketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFd < 0) {
        perror("socket");
        return false;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr) <= 0) {
        perror("inet_pton");
        CLOSE_SOCKET(socketFd);
        socketFd = -1;
        return false;
    }

    if (connect(socketFd, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
#ifdef _WIN32
        int err = WSAGetLastError();
        if (!(mode == SocketMode::NonBlocking && err == WSAEWOULDBLOCK)) {
            perror("connect");
            CLOSE_SOCKET(socketFd);
            socketFd = -1;
            return false;
        }
#else
        if (!(mode == SocketMode::NonBlocking && errno == EINPROGRESS)) {
            perror("connect");
            CLOSE_SOCKET(socketFd);
            socketFd = -1;
            return false;
        }
#endif
    }

    // Set socket mode
#ifdef _WIN32
    u_long m = (mode == SocketMode::NonBlocking ? 1 : 0);
    ioctlsocket(socketFd, FIONBIO, &m);
#else
    int flags = fcntl(socketFd, F_GETFL, 0);
    if (mode == SocketMode::NonBlocking)
        fcntl(socketFd, F_SETFL, flags | O_NONBLOCK);
    else
        fcntl(socketFd, F_SETFL, flags & ~O_NONBLOCK);
#endif

    connected = true;
    return true;
}

void NetworkClient::disconnect() {
    if (!connected) return;
    CLOSE_SOCKET(socketFd);
    socketFd = -1;
    connected = false;
}

bool NetworkClient::isConnected() const {
    return connected;
}

bool NetworkClient::send(const void* data, size_t size) {
    if (!connected) return false;
    const char* buffer = static_cast<const char*>(data);
    size_t totalSent = 0;

    while (totalSent < size) {
#ifdef _WIN32
        int sent = ::send(socketFd, buffer + totalSent, (int)(size - totalSent), 0);
        if (sent < 0) {
            int err = WSAGetLastError();
            if (mode == SocketMode::NonBlocking && err == WSAEWOULDBLOCK) continue;
#else
        ssize_t sent = ::send(socketFd, buffer + totalSent, size - totalSent, 0);
        if (sent < 0) {
            if (mode == SocketMode::NonBlocking && (errno == EWOULDBLOCK || errno == EAGAIN)) continue;
#endif
            perror("send");
            disconnect();
            return false;
        }
        totalSent += sent;
    }
    return true;
}

int NetworkClient::receive(void* buffer, size_t size) {
    if (!connected) return -1;

#ifdef _WIN32
    int received = ::recv(socketFd, static_cast<char*>(buffer), (int)size, 0);
#else
    ssize_t received = ::recv(socketFd, buffer, size, 0);
#endif

    if (received == 0) { disconnect(); return -1; }

    if (received < 0) {
#ifdef _WIN32
        int err = WSAGetLastError();
        if (mode == SocketMode::NonBlocking && err == WSAEWOULDBLOCK) return 0;
#else
        if (mode == SocketMode::NonBlocking && (errno == EWOULDBLOCK || errno == EAGAIN)) return 0;
#endif
        perror("recv");
        disconnect();
        return -1;
    }

    return static_cast<int>(received);
}