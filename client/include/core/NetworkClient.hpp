#ifndef NETWORKCLIENT_HPP
#define NETWORKCLIENT_HPP

#include <string>

class NetworkClient {
public:
    NetworkClient();
    ~NetworkClient();

    NetworkClient(const NetworkClient&) = delete;
    NetworkClient& operator=(const NetworkClient&) = delete;

    bool connectTo(const std::string& ip, int port);
    void disconnect();

    bool isConnected() const;

    bool send(const void* data, size_t size);
    int receive(void* buffer, size_t size);

private:
    int socketFd;
    bool connected;
};


#endif