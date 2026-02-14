#ifndef PLAYERCONNECTION_HPP
#define PLAYERCONNECTION_HPP

#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <functional>

//1 object means 1 player connection from client -> server
//need callback runs for this
//can form shared pointer to itself
class PlayerConnection : public std::enable_shared_from_this<PlayerConnection> {
public:
    explicit PlayerConnection(int socket);
    ~PlayerConnection();

    //Make non copyable
    PlayerConnection(const PlayerConnection&) = delete;
    PlayerConnection& operator=(const PlayerConnection&) = delete;

    bool start();
    void stop();
    int getSocket() const;

    // Event: called when a message is received
    std::function<void(const std::vector<char>&)> onMessageReceived;

private:
    //reads incoming data from client's socket and stores
    void readLoop();

    int clientSocket;

    //thread to run readLoop() on
    std::thread readThread;
    std::mutex writeMutex;
    std::atomic<bool> running;
};

#endif