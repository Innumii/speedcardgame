#ifndef CONNECTING_HPP
#define CONNECTING_HPP

#include <string>
#include <SDL2/SDL.h>

//Should make a general parent class for all states at some point.

class Game;

class Connecting {
public:
    //ip and port is for the server client connects to
    Connecting(const std::string& ip, int port);

    void handleEvents(Game& game, const SDL_Event& event);
    void update(Game& game);
    void render(Game& game);

private:
    std::string serverIp;
    int serverPort;
    //might need to change to can retry connection?
    bool attemptedConnection = false;
};

#endif