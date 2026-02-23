#include "states/Waiting.hpp"
#include "core/Game.hpp"
#include <iostream>

void Waiting::handleEvents(Game& game, const SDL_Event& event) {
    if (event.type == SDL_QUIT) {
        game.setNextState(GameState::Quit);
        return;
    }

    // Allow cancel back to title
    if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
        std::cout << "Cancelled waiting\n";
        game.setNextState(GameState::Title);
    }
}

void Waiting::update(Game& game) {
    auto& net = game.getNetworkClient();
    // Non-blocking receive
    char buffer[512];
    int received = net.receive(buffer, sizeof(buffer));
    if (received == -1) {
        std::cerr << "Server disconnected\n";
        game.setNextState(GameState::Title);
        return;
    }

    if (received > 0) {
        // Append safely
        recvBuffer.append(buffer, received);

        // Simple protocol: if server sends "START", begin match
        if (recvBuffer.find("START") != std::string::npos) {
            std::cout << "Match starting!\n";
            recvBuffer.clear();
            game.setNextState(GameState::Playing);
            return;
        }

        // Prevent unbounded growth
        if (recvBuffer.size() > 4096) {
            recvBuffer.erase(0, recvBuffer.size() - 1024);
        }
    }

    // received == 0 → no data available, just keep waiting
}

void Waiting::render(const Game& game) {
    SDL_Renderer* renderer = game.getRenderer();

    // Background
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
    SDL_RenderClear(renderer);

    // Center panel
    SDL_Rect rect{200, 200, 400, 100};
    SDL_SetRenderDrawColor(renderer, 200, 180, 60, 255); // gold
    SDL_RenderFillRect(renderer, &rect);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &rect);

    // Optional: text later using your font system
    // RenderText::drawCentered(renderer, game.getUIFonts(), "Waiting for opponent...", rect);

    SDL_RenderPresent(renderer);
}