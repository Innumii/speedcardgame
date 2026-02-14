#ifndef MATCHMAKER_HPP
#define MATCHMAKER_HPP

#include <memory>
#include <mutex>
#include <queue>
#include "core/MatchSession.hpp"
#include "net/PlayerConnection.hpp"

class Matchmaker {
public:
    Matchmaker() = default;
    ~Matchmaker() = default;

    Matchmaker(const Matchmaker&) = delete;
    Matchmaker& operator=(const Matchmaker&) = delete;

    //takes shared ptr by reference
    //shared ptr ensures player object stays alive while in queue
    //otherwise, PlayerConnection obj will go out of scope when u enqueue->dangling ptr->crash
    void enqueuePlayer(const std::shared_ptr<PlayerConnection>& player);
    void removePlayer(const std::shared_ptr<PlayerConnection>& player);

    
private:
    void tryCreateMatch();

    //queue storing shared ptrs of PlayerConnection objects
    std::queue<std::shared_ptr<PlayerConnection>> playerQueue;
    //mutex protects queue
    std::mutex queueMutex;
};

#endif