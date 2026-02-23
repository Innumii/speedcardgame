#include "states/Waiting.hpp"
#include "render/RenderButton.hpp"
#include "render/RenderCard.hpp"
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

    if (event.type == SDL_MOUSEMOTION) {
        int x = event.motion.x;
        int y = event.motion.y;
        acceptHovered = (x >= acceptRect.x && x <= acceptRect.x + acceptRect.w &&
                        y >= acceptRect.y && y <= acceptRect.y + acceptRect.h);
        declineHovered = (x >= declineRect.x && x <= declineRect.x + declineRect.w &&
                        y >= declineRect.y && y <= declineRect.y + declineRect.h);
    }

    if (event.type == SDL_MOUSEBUTTONDOWN && matchFound) {
        int x = event.button.x;
        int y = event.button.y;

        if (acceptHovered) {
            acceptPressed = true;
        }
        if (declineHovered) {
            declinePressed = true;
        }
    }

    if (event.type == SDL_MOUSEBUTTONUP && matchFound) {
        int x = event.button.x;
        int y = event.button.y;

        if (acceptPressed && acceptHovered) {
            game.getNetworkClient().sendString("MATCH_ACCEPT\n");
            accepted = true;
            waitingForOpponent = false;
        }
        //change so that decline -> return to title screen
        if (declinePressed && declineHovered) {
            game.getNetworkClient().sendString("MATCH_DECLINE\n");
            declined = true;
            matchFound = false;
            waitingForOpponent = false;
        }
        acceptPressed = false;
        declinePressed = false;
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

            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

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
            else if (line.rfind("OPPONENT_INFO|", 0) == 0) {
                const std::size_t firstSep = line.find('|');
                const std::size_t secondSep = line.find('|', firstSep + 1);
                if (firstSep != std::string::npos && secondSep != std::string::npos) {
                    const std::string idToken = line.substr(firstSep + 1, secondSep - (firstSep + 1));
                    const std::string username = line.substr(secondSep + 1);

                    try {
                        const int opponentId = std::stoi(idToken);
                        game.setOpponentPlayerInfo(opponentId, username);
                        std::cout << "Opponent paired: id=" << opponentId << ", username=" << username << "\n";
                    } catch (...) {
                        std::cerr << "Failed to parse OPPONENT_INFO message: " << line << "\n";
                    }
                }
            }
            else if (line.rfind("OPPONENT_COUNTS|", 0) == 0) {
                const std::size_t firstSep = line.find('|');
                const std::size_t secondSep = line.find('|', firstSep + 1);
                if (firstSep != std::string::npos && secondSep != std::string::npos) {
                    const std::string handToken = line.substr(firstSep + 1, secondSep - (firstSep + 1));
                    const std::string deckToken = line.substr(secondSep + 1);

                    try {
                        const std::size_t handCount = static_cast<std::size_t>(std::stoul(handToken));
                        const std::size_t deckCount = static_cast<std::size_t>(std::stoul(deckToken));
                        game.setOpponentCounts(handCount, deckCount);
                    } catch (...) {
                        std::cerr << "Failed to parse OPPONENT_COUNTS message: " << line << "\n";
                    }
                }
            }
            else if (line == "MATCH_START") {
                RenderCard::preloadCommonCardImages(game.getRenderer());
                game.setNextState(GameState::Playing);
                return;
            }
        }

    }
}

void Waiting::render(const Game& game) {
    SDL_Renderer* renderer = game.getRenderer();
    if (!renderer) return;

    // Background
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
    SDL_RenderClear(renderer);

    // Center panel
    SDL_Rect panel{200, 200, 400, 100};
    SDL_SetRenderDrawColor(renderer, 200, 180, 60, 255); // gold
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &panel);

    // If match found, render buttons using RenderButton
    if (matchFound) {
        if (accepted) {

        } else {
            SDL_Color acceptFill   = {0, 200, 0, 255};   // green
            SDL_Color declineFill  = {200, 0, 0, 255};   // red
            SDL_Color borderColor  = {255, 255, 255, 255};
            SDL_Color textColor    = {255, 255, 255, 255};

            RenderButton::drawButton(renderer, acceptRect, "Accept",
                                        game.getUIFonts().large,
                                        acceptFill, borderColor, textColor,
                                        acceptHovered, acceptPressed);

            // Decline button
            RenderButton::drawButton(renderer, declineRect, "Decline",
                                        game.getUIFonts().large,
                                        declineFill, borderColor, textColor,
                                        declineHovered, declinePressed);
        }

    }

    // Present everything
    SDL_RenderPresent(renderer);
}
