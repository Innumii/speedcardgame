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
#include <render/RenderBackdrop.hpp>

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

        connectionThread = std::thread([this, &game]() {
            bool ok = game.getNetworkClient().connectTo(serverIp, serverPort);
            success = ok;
            finished = true;
        });
    }

    if (finished) {
        if (success) {
            std::cout << "Connection succeeded!\n";

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
    RenderBackdrop::drawBackgroundWithVignette(
        renderer,
        screenW,
        screenH,
        Theme::BG,                              // base background color
        Theme::Loading::VIGNETTE_COLOR,        // vignette color
        Theme::Loading::VIGNETTE_LAYERS,       // layers
        Theme::Loading::VIGNETTE_ALPHA_FALLOFF,// alpha falloff
        Theme::Loading::VIGNETTE_MAX_ALPHA     // max alpha
    );

    // ── panel layout (centered, scaled) ──────────────────────────────
    const float scale = std::min(
        static_cast<float>(screenW) / 1200.0F,
        static_cast<float>(screenH) / 850.0F);

    const int panelW = static_cast<int>(420 * scale);
    const int panelH = static_cast<int>(140 * scale);

    SDL_Rect panel{
        (screenW - panelW) / 2,
        (screenH - panelH) / 2,
        panelW,
        panelH
    };

    RenderBanner::drawBanner(
        renderer,
        panel,
        "Connecting to Server...",
        uiFonts.large,
        Theme::BANNER_FILL,
        Theme::BANNER_BORDER,
        Theme::BANNER_TEXT,
        Theme::BANNER_GLOW
    );
}