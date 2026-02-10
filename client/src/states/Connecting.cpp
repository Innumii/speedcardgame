#include "states/Connecting.hpp"
#include <SDL2/SDL.h>
#include <iostream>
#include <thread>
#include "core/Game.hpp"

Connecting::Connecting(const std::string& ip, int port):serverIp(ip),serverPort(port) {

}

Connecting::~Connecting() {
    if (connectionThread.joinable()) {
        connectionThread.join();
    }
}

void Connecting::handleEvents(Game& game, const SDL_Event& event) {
    if (event.type == SDL_QUIT) {
        game.setNextState(GameState::Quit);
    }

    
}

void Connecting::update(Game& game) {
    if (!attemptedConnection) {
        attemptedConnection = true;
        connectionThread = std::thread([this, &game](){
            bool ok = game.getNetworkClient().connectTo(serverIp, serverPort);
            success = ok;
            finished = true;
        });
    }

    // Main thread polls flags and updates UI, no blocking
    if (finished) {
        if (success) {
            // Connected successfully, rectangle turns green
            std::cout << "Connected!\n";
            game.setNextState(GameState::Waiting);
        } else {
            std::cerr << "Failed to connect\n";
            game.setNextState(GameState::Title);
        }

        finished = false; // reset for retry if needed
    }
}

void Connecting::render(Game& game) {
    SDL_Renderer* renderer = game.getRenderer();

    // Clear screen
    SDL_SetRenderDrawColor(renderer, 25, 25, 25, 255);
    SDL_RenderClear(renderer);

    // Always blue rectangle while connecting
    SDL_Rect rect{200, 200, 400, 100};
    SDL_SetRenderDrawColor(renderer, 80, 120, 200, 255); // blue
    SDL_RenderFillRect(renderer, &rect);

    // Draw outline
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &rect);

    SDL_RenderPresent(renderer);
}
