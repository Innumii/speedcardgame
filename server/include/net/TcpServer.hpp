#ifndef TCPSERVER_HPP
#define TCPSERVER_HPP

#include <vector>
#include <string>

class TcpServer {
public:
    explicit TcpServer(int port);
    bool start();
    void stop();
    void acceptClients();
    void handleClient(int clientSocket);
private:
    int listenPort;
    int listenSocket;
    std::vector<int> clientSockets;
};

#endif