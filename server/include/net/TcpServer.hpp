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
#include <condition_variable>
#include <queue>

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
    void removeClient(const std::shared_ptr<PlayerConnection>& player);

    //handling disconnection of PlayerConnection objects (user dc)
    void enqueueDisconnect(const std::shared_ptr<PlayerConnection>& player);
    void disconnectLoop();


private:
    void acceptClients();

    int listenPort;
    int listenSocket;

    std::atomic<bool> running;

    //handle connection created, chuck into clients
    std::thread acceptThread;
    std::mutex clientsMutex;
    std::vector<std::shared_ptr<PlayerConnection>> clients;
    
    //handle disconnecting, constantly dequeue 
    std::queue<std::shared_ptr<PlayerConnection>> disconnectQueue;
    std::mutex disconnectMutex;
    std::condition_variable disconnectCv;

    std::thread disconnectThread;
    bool disconnectRunning{false};
};

#endif