#ifndef CONNECTING_HPP
#define CONNECTING_HPP

#include <string>
#include <SDL2/SDL.h>
#include <atomic>
#include <thread>

//Should make a general parent class for all states at some point.

class Game;

class Connecting {
public:
    //ip and port is for the server client connects to
    Connecting(const std::string& ip, int port);
    ~Connecting();

    void handleEvents(Game& game, const SDL_Event& event);
    void update(Game& game);
    void render(const Game& game);

private:
    std::string serverIp;
    int serverPort;
    std::atomic<bool> finished{false};
    std::atomic<bool> success{false};
    std::thread connectionThread;
    //might need to change to can retry connection?
    bool started = false;
};

#endif