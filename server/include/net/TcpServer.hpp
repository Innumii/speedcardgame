#ifndef TCPSERVER_HPP
#define TCPSERVER_HPP

//dynamic array
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <memory>

class PlayerConnection;

class TcpServer {
public:
    explicit TcpServer(int port);
    ~TcpServer();
    
    //ensure u cannot copy a TcpServer
    TcpServer(const TcpServer&) = delete; 
    //ensure u cannot assign 1 TcpServer object to another
    TcpServer& operator = (const TcpServer&) = delete;

    bool start();
    void stop();
    //Event callback: called when a new client connects
    std::function<void(std::shared_ptr<PlayerConnection>)> onClientConnected;
private:
    void acceptClients();

    int listenPort;
    int listenSocket;

    std::atomic<bool> running;
    std::thread acceptThread;

    std::mutex clientsMutex;
    std::vector<int> clientSockets; // raw sockets, wrapped in PlayerConnection later
};

#endif