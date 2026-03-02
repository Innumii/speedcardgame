#ifndef GAMESERVER_HPP
#define GAMESERVER_HPP
#include "net/TcpServer.hpp"
#include "core/Matchmaker.hpp"
#include "core/MatchManager.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <unordered_map>

class TcpServer;
class Matchmaker;

class GameServer {
public:
    explicit GameServer(int port);
    bool start();
    void stop();
    void waitForShutdown();
    const std::unordered_map<int, std::shared_ptr<ServerCard>>& getAllCards() const { return availableCards; }
private:
    int port;

    //Smart ptrs (prevent memory leak fkery)
    std::unique_ptr<TcpServer> tcpServer;
    std::unique_ptr<Matchmaker> matchmaker;
    std::unique_ptr<MatchManager> matchManager;


    std::atomic<bool> running;
    std::mutex shutdownMutex;
    std::condition_variable shutdownCv;

    //all cards in the game, ever
    std::unordered_map<int, std::shared_ptr<ServerCard>> availableCards;
    bool loadAvailableCardsFromService();

};

#endif