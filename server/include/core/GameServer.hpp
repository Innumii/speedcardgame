#ifndef GAMESERVER_HPP
#define GAMESERVER_HPP
#include "net/TcpServer.hpp"
#include "core/Matchmaker.hpp"
#include "core/MatchManager.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <condition_variable>

class TcpServer;
class Matchmaker;

class GameServer {
public:
    explicit GameServer(int port);
    bool start();
    void stop();
    void waitForShutdown();
private:
    int port;

    //Smart ptrs (prevent memory leak fkery)
    std::unique_ptr<TcpServer> tcpServer;
    std::unique_ptr<Matchmaker> matchmaker;
    std::unique_ptr<MatchManager> matchManager;


    std::atomic<bool> running;
    std::mutex shutdownMutex;
    std::condition_variable shutdownCv;
};

#endif