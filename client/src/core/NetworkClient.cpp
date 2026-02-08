#include "core/NetworkClient.hpp"

#ifdef _WIN32
    #include <Winsock2.h>
    #include <Ws2tcpip.h>
    #pragma comment(lib, "Ws2_32.lib")
    //create alias as Winsock returns int datatype
    using ssize_t = int;
    #define CLOSE_SOCKET(s) closesocket(s)

#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #define CLOSE_SOCKET(s) close(s)
#endif

#include <cstring>
#include <cerrno>
#include <iostream>

NetworkClient::NetworkClient():socketFd(-1), connected(false) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        std::cerr<< "WSAStartup failed\n";
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
    if (connected) {
        return false;
    }

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

    if (connect(socketFd, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("connect");
        CLOSE_SOCKET(socketFd);
        socketFd = -1;
        return false;
    }

    connected = true;
    return true;
}

void NetworkClient::disconnect() {
    if (!connected) {
        return;
    }
    CLOSE_SOCKET(socketFd);
    socketFd = -1;
    connected = false;
}

bool NetworkClient::isConnected() const {
    return connected;
}

bool NetworkClient::send(const void* data, size_t size) {
    if (!connected) {
        return false;
    }

    const char* buffer = static_cast<const char*>(data);
    size_t totalSent = 0;

    while (totalSent < size) {
        ssize_t sent = ::send(
            socketFd,
            buffer + totalSent,
            size - totalSent,
            0
        );

        if (sent<=0) {
            perror("send");
            disconnect();
            return false;
        }

        totalSent+=sent;
    }

    return true;
}

//2 meaningful return values
/* >0 -> no. bytes received
   -1 -> dead connection
*/
int NetworkClient::receive(void* buffer, size_t size) {
    if (!connected) {
        return -1;
    }

    //recv will return 1 of 3 values (>0, 0, <0)
    ssize_t received = ::recv(socketFd, buffer, size, 0);

    if (received <= 0) { //connection closed by peer gracefully
        if (received < 0) { // error occurred
            perror("recv");
        }
            disconnect();
            return -1;
    }
    return static_cast<int>(received);
}