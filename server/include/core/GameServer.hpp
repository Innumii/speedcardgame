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
    const std::vector<std::unique_ptr<Card>>& getAllCards() const { return availableCards; }

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
    std::vector<std::unique_ptr<Card>> availableCards;
    bool loadAvailableCardsFromService();

    bool sendHttp(const std::string& host, int port, const std::string& method,
              const std::string& path, const std::string& body,
              int& statusCode, std::string& responseBody);

};

#endif