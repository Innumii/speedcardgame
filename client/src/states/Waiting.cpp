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
    char buffer[512];
    int received = net.receive(buffer, sizeof(buffer));

    if (received == -1) {
        std::cerr << "Server disconnected\n";
        game.setNextState(GameState::Title);
        return;
    }

    if (received > 0) {
        recvBuffer.append(buffer, received);
        size_t pos;
        while ((pos = recvBuffer.find('\n')) != std::string::npos) {
            std::string line = recvBuffer.substr(0, pos);
            recvBuffer.erase(0, pos + 1); // remove processed line

            if (line == "MATCH_FOUND") {
                matchFound = true;
                waitingForOpponent = true;
                accepted = false;
                declined = false;
                std::cout << "Match found! Waiting for your response...\n";
            }
            else if (line == "MATCH_CANCELLED") {
                matchFound = false;
                waitingForOpponent = false;
                std::cout << "Match cancelled, back to queue\n";
            }
            else if (line == "MATCH_START") {
                game.setNextState(GameState::Playing);
                return;
            }
        }

    }
}


void Waiting::render(const Game& game) {
    SDL_Renderer* renderer = game.getRenderer();

    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
    SDL_RenderClear(renderer);

    SDL_Rect panel{200, 200, 400, 100};
    SDL_SetRenderDrawColor(renderer, 200, 180, 60, 255);
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &panel);

    // If match found, draw buttons
    if (matchFound) {
        SDL_Rect acceptRect{220, 250, 150, 50};
        SDL_Rect declineRect{430, 250, 150, 50};

        // Accept button
        SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255); // green
        SDL_RenderFillRect(renderer, &acceptRect);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer, &acceptRect);

        // Decline button
        SDL_SetRenderDrawColor(renderer, 200, 0, 0, 255); // red
        SDL_RenderFillRect(renderer, &declineRect);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer, &declineRect);

        // Optional: render button text using your font system
        // RenderText::drawCentered(renderer, fonts, "Accept", acceptRect);
        // RenderText::drawCentered(renderer, fonts, "Decline", declineRect);
    }

    SDL_RenderPresent(renderer);
}
