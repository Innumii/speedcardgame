#include "states/Waiting.hpp"
#include "render/RenderButton.hpp"
#include "render/RenderText.hpp"
#include "render/RenderBanner.hpp"
#include "render/Theme.hpp"
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

    if (event.type == SDL_MOUSEBUTTONDOWN) {
        int x = event.button.x;
        int y = event.button.y;

        if (acceptHovered) {
            acceptPressed = true;
        }
        if (declineHovered) {
            declinePressed = true;
        }
    }

    if (event.type == SDL_MOUSEBUTTONUP) {
        int x = event.button.x;
        int y = event.button.y;

        if (acceptPressed && acceptHovered) {
            if (matchFound) {
                game.getNetworkClient().sendString("MATCH_ACCEPT\n");
                accepted = true;
                waitingForOpponent = false;
            }
        }
        //change so that decline -> return to title screen
        if (declinePressed && declineHovered) {
            declined = true;
            matchFound = false;
            waitingForOpponent = false;
            //return to title
            game.setNextState(GameState::Title);
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
                auto& playingState = game.getPlayingState();

                //hardcoded temp Player data 
                playingState.setupPlayers(std::move(game.getPlayer()), Player());
                std::cout << "Entering Playing state...\n";
                
                game.setNextState(GameState::Playing);
                return;
            }
        }

    }
}

void Waiting::render(const Game& game) {
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

    const int btnW = 180;
    const int btnH = 45;
    const int spacing = 15;

    int btnX = panel.x + (panel.w - btnW) / 2;
    int firstBtnY  = panel.y + panel.h + spacing + 20; // 20px padding from bottom
    int secondBtnY = firstBtnY + btnH + spacing;
    declineRect = { btnX, firstBtnY, btnW, btnH };

    SDL_Color acceptFill  = Theme::BTN_START;
    SDL_Color declineFill = Theme::BTN_QUIT;
    SDL_Color border      = Theme::BTN_BORDER;
    SDL_Color text        = Theme::BTN_TEXT;
    std::string msg = "Finding Match...";

    if (matchFound) {
        msg = "Match Found!";
        if (accepted) {
            msg = "Waiting for Opponent...";
        }
        else {
            acceptRect  = { btnX, firstBtnY, btnW, btnH };
            declineRect = { btnX, secondBtnY, btnW, btnH };

            RenderButton::drawButton(
                renderer, acceptRect, "Accept",
                uiFonts.large,
                acceptFill, border, text,
                acceptHovered, acceptPressed
            );

            
        }
    }
    //draw banner
    RenderBanner::drawBanner(
        renderer,
        panel,
        msg,
        uiFonts.large,
        panelFill,
        borderColor,
        textColor,
        Theme::BANNER_GLOW
    );
    RenderButton::drawButton(
        renderer, declineRect, "Back to Title",
        uiFonts.large,
        declineFill, border, text,
        declineHovered, declinePressed
    );

}