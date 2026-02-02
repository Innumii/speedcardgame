#ifndef SERVER_HPP
#define SERVER_HPP
#include "net/TcpServer.hpp"

class Server {
public:
    explicit Server(int port);
    void run();
private:
    TcpServer tcpServer;
};

#endif