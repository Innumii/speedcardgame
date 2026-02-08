#ifndef CONNECTING_HPP
#define CONNECTING_HPP

#include "core/Game.hpp"
#include <string>

//Should make a general parent class for all states at some point.

class Connecting {
public:
    Connecting(const std::string& ip, int port);

    void handleEvents(Game& game, const SDL_Event& event);
    void update(Game& game);
    void render(Game& game);

private:
    std::string serverIp;
    int serverPort;
    //might need to change to can retry connection?
    bool attemptedConnection = false;
}

#endif