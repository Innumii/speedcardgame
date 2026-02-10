#include "states/Connecting.hpp"
#include <SDL2/SDL.h>
#include <iostream>
#include <thread>
#include "core/Game.hpp"

Connecting::Connecting(const std::string& ip, int port):serverIp(ip),serverPort(port) {

}

void Connecting::handleEvents(Game& game, const SDL_Event& event) {
    if (event.type == SDL_QUIT) {
        game.setNextState(GameState::Quit);
    }

    
}

void Connecting::update(Game& game) {
    if (!attemptedConnection) {
        attemptedConnection = true;

        // start async thread
        std::thread([this, &game]() {
            bool ok = game.getNetworkClient().connectTo(serverIp, serverPort);
            if (ok) {
                std::cout << "Connected to Server\n";
                game.setNextState(GameState::Waiting);
            } else {
                std::cerr << "Failed to Connect to Server\n";
                game.setNextState(GameState::Title);
            }
        }).detach(); // detached thread
    }
}

void Connecting::render(Game& game) {
    SDL_Renderer* renderer = game.getRenderer();

    SDL_SetRenderDrawColor(renderer, 25, 25, 25, 255);
    SDL_RenderClear(renderer);

    // Render "Connecting..." text or simple rectangle
    // (You can later use TTF to render actual text)
    SDL_Rect rect{200, 200, 400, 100};
    SDL_SetRenderDrawColor(renderer, 180, 40, 40, 70);
    SDL_RenderFillRect(renderer, &rect);

}