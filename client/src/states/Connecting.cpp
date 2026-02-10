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

        // Copy IP/port for thread
        std::string ipCopy = serverIp;
        int portCopy = serverPort;

        // Start background thread for probe connection
        connectionThread = std::thread([ipCopy, portCopy, this]() {
            NetworkClient tempClient;
            bool ok = tempClient.connectTo(ipCopy, portCopy);

            // Only store result in atomic flags, no Game access
            success = ok;
            finished = true;
        });
    }

    // Main thread polls result
    if (finished) {
        if (success) {
            std::cout << "Connection succeeded!\n";

            // Now safely connect persistent client on main thread
            if (!game.getNetworkClient().isConnected()) {
                bool ok = game.getNetworkClient().connectTo(serverIp, serverPort);
                if (!ok) {
                    std::cerr << "Failed to initialize persistent NetworkClient\n";
                    game.setNextState(GameState::Title);
                    finished = false;
                    return;
                }
            }

            game.setNextState(GameState::Waiting);
        } else {
            std::cerr << "Failed to connect\n";
            game.setNextState(GameState::Title);
        }

        finished = false;
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
