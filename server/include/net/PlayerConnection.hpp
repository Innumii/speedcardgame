#ifndef PLAYERCONNECTION_HPP
#define PLAYERCONNECTION_HPP

#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <functional>
#include <queue>
#include <string>
#include <openssl/ssl.h>
#include <openssl/err.h>

//1 object means 1 player connection from client -> server
//need callback runs for this
//can form shared pointer to itself
class PlayerConnection : public std::enable_shared_from_this<PlayerConnection> {
public:
    explicit PlayerConnection(int socket, SSL_CTX* ctx);
    ~PlayerConnection();

    //Make non copyable
    PlayerConnection(const PlayerConnection&) = delete;
    PlayerConnection& operator=(const PlayerConnection&) = delete;

    bool start();
    void stop();
    int getSocket() const;

    //actions
    bool send(const std::string& msg);        // send message to client
    bool pollMessage(std::string& outMsg);    // check if messages received
    bool isAlive() const;   

    // Event: called when a message is received
    std::function<void(const std::vector<char>&)> onMessageReceived;
    std::function<void()> onDisconnected;

    void setPlayerInfo(int id, const std::string& name);

    int getPlayerId() const;
    const std::string& getUsername() const;
private:
    //reads incoming data from client's socket and stores
    void readLoop();
    int playerId = -1;
    std::string username;

    int clientSocket;

    //thread to run readLoop() on
    std::thread readThread;
    
    //queue and mutex
    std::queue<std::string> messageQueue;
    std::mutex queueMutex;

    //SSL
    SSL* ssl{nullptr};
    SSL_CTX* sslCtx;

    std::atomic<bool> running;

    std::mutex stopMutex;
    bool stopped = false;

};

#endif