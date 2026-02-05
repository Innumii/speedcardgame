#ifndef TCPSERVER_HPP
#define TCPSERVER_HPP

//dynamic array
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>


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
    // void handleClient(int clientSocket);
private:
    int listenPort;
    int listenSocket;
    std::atomic<bool> running;

    //list of clients connected
    std::vector<int> clientSockets;
    std::mutex clientsMutex; //lock

    //delegate accepting clients to its own thread
    std::thread acceptThread; 
    void acceptClients();
};

#endif