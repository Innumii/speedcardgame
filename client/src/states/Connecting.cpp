#include "states/Connecting.hpp"
#include "render/RenderButton.hpp"
#include "render/RenderText.hpp"
#include "render/RenderBanner.hpp"
#include "render/Theme.hpp"
#include <SDL2/SDL.h>
#include <iostream>
#include <sstream>
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
    if (!started) {
        started = true;

        // Launch async connect ONCE
        connectionThread = std::thread([this, &game]() {
            bool ok = game.getNetworkClient().connectTo(serverIp, serverPort);
            success = ok;
            finished = true;
        });
    }

    // Main thread polls result
    if (finished) {
        if (success) {
            std::cout << "Connection succeeded!\n";
            // sends player data to server and waits for response in Waiting state

            const int playerId = game.getPlayerId();
            const std::string& username = game.getPlayerUsername();

            auto escapeJson = [](const std::string& input) {
                std::string escaped;
                escaped.reserve(input.size());
                for (char ch : input) {
                    switch (ch) {
                        case '\\': escaped += "\\\\"; break;
                        case '"': escaped += "\\\""; break;
                        case '\n': escaped += "\\n"; break;
                        case '\r': escaped += "\\r"; break;
                        case '\t': escaped += "\\t"; break;
                        default: escaped += ch; break;
                    }
                }
                return escaped;
            };

            std::ostringstream payload;
            payload << "{\"type\":\"player_info\",\"playerId\":" << playerId
                    << ",\"username\":\"" << escapeJson(username) << "\"}\n";
            const std::string message = payload.str();

            if (!game.getNetworkClient().send(message.data(), message.size())) {
                std::cerr << "Failed to send player info to server\n";
                game.setNextState(GameState::Title);
                return;
            }
            std::cout << "sent player info\n";
            game.setNextState(GameState::Waiting);
        } else {
            std::cerr << "Connection failed\n";
            game.setNextState(GameState::Title);
        }
    }
}

void Connecting::render(const Game& game) {
    SDL_Renderer* renderer = game.getRenderer();
    if (!renderer) return;

    const auto& uiFonts = game.getUIFonts();

    int screenW, screenH;
    SDL_GetRendererOutputSize(renderer, &screenW, &screenH);

    // ── background ─────────────────────────────
    SDL_SetRenderDrawColor(renderer, Theme::BG.r, Theme::BG.g, Theme::BG.b, 255);
    SDL_RenderClear(renderer);

    // ── panel layout (centered) ───────────────
    const int panelW = 420;
    const int panelH = 140; // taller to fit 2 buttons stacked

    SDL_Rect panel{
        (screenW - panelW) / 2,
        (screenH - panelH) / 2,
        panelW,
        panelH
    };

    SDL_Color panelFill   = Theme::BANNER_FILL;
    SDL_Color borderColor = Theme::BANNER_BORDER;
    SDL_Color textColor   = Theme::BANNER_TEXT;

    RenderBanner::drawBanner(
        renderer,
        panel,
        "Connecting to Server...",
        uiFonts.large,
        panelFill,
        borderColor,
        textColor,
        Theme::BANNER_GLOW
    );
    
}
