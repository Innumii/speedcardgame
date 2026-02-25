#ifndef NETWORKCLIENT_HPP
#define NETWORKCLIENT_HPP

#include <string>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <optional>


#ifdef _WIN32
#include <Winsock2.h>
#include <Ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#define CLOSE_SOCKET(s) closesocket(s)
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#define CLOSE_SOCKET(s) close(s)
#endif

class NetworkClient {
public:
    enum class SocketMode { Blocking, NonBlocking };

    explicit NetworkClient(SocketMode mode = SocketMode::Blocking);
    ~NetworkClient();

    bool connectTo(const std::string& ip, int port);
    void disconnect();
    bool isConnected() const;

    bool send(const void* data, size_t size);
    bool sendString(const std::string& msg);
    int receive(void* buffer, size_t size); // >0=bytes, 0=no data (non-blocking), -1=error/closed

    //game actions
    bool sendPlayCard(int handIndex, int lane, std::optional<int> targetId);
    bool sendDiscardCard(int handIndex);

private:
    int socketFd;
    bool connected;
    SocketMode mode;

    //SSL
    SSL_CTX* sslCtx{nullptr};
    SSL* ssl{nullptr};
    bool initOpenSSL();
    void cleanupSSL();
};


#endif