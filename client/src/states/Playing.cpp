#include "states/Playing.hpp"

#include "core/Game.hpp"
#include "render/RenderPLaying.hpp"
#include "render/RenderText.hpp"
#include "gameplay/LocalAuthority.hpp"

#include <SDL2/SDL.h>
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
    SDL_Rect playZoneBand{0, 0, 0, 0};
}

// -------------------------
// Helpers
// -------------------------
bool Playing::pointInRect(const SDL_Rect& rect, int x, int y) {
    return x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h;
}

// Resolves pending action when player clicks a target
bool Playing::resolvePendingActionAt(int x, int y) {
    if (!pendingAction.active) return false;

    // Loop through all play slots to detect target
    for (std::size_t slot = 0; slot < playSlots.size(); ++slot) {
        if (pointInRect(playSlots[slot], x, y)) {
            pendingAction.targetId = static_cast<int>(slot); // target = lane index
            authority->playCard(pendingAction.handIndex, static_cast<int>(slot), pendingAction.targetId);
            pendingAction = PendingActionState{};
            return true;
        }
    }

    // Optionally handle opponent lanes
    SDL_Rect opponentRect = playSlots[0];
    opponentRect.y -= 200; // offset for opponent
    if (pointInRect(opponentRect, x, y)) {
        pendingAction.targetId = 0; // example: opponent ID
        authority->playCard(pendingAction.handIndex, 0, pendingAction.targetId);
        pendingAction = PendingActionState{};
        return true;
    }

    return false;
}

// -------------------------
// Setup
// -------------------------
Playing::Playing(int drawIntervalSeconds)
    : drawIntervalSeconds(drawIntervalSeconds) {}

Playing::~Playing() {
    RenderText::closeFonts(fonts);
    RenderText::shutdownTtf();
}

void Playing::setup(Game& game) {
    renderer = game.getRenderer();
    if (!renderer) throw std::runtime_error("Renderer not available");

    drag = DragState{};
    hoverIndex = static_cast<std::size_t>(-1);
    hoverStartTick = 0;
    menuOpen = false;
    surrendered = false;
    animationQueue.clear();
    pendingAction = PendingActionState{};
    lastDrawTick = SDL_GetTicks();
    running = true;

    if (!RenderText::ensureTtfReady()) {
        throw std::runtime_error(std::string("TTF_Init failed: ") + TTF_GetError());
    }

    fonts = RenderText::loadFonts("assets/font.TTF", 14, 12, 24);
    if (!fonts.large || !fonts.small) {
        RenderText::closeFonts(fonts);
        throw std::runtime_error("Failed to load fonts");
    }

    if (!authority) {
        // fallback to local authority if none provided
        authority = std::make_unique<LocalAuthority>(game.getNetworkClient());
    }

    // Local hand/board initialization will come from server
}

// -------------------------
// Event handling
// -------------------------
void Playing::handleEvents(Game& game, const SDL_Event& event) {
    if (!renderer) return;

    int screenW = 0, screenH = 0;
    SDL_GetRendererOutputSize(renderer, &screenW, &screenH);
    computeZones(screenW, screenH);
    computeUiRects(screenW, screenH);

    const int mx = event.button.x;
    const int my = event.button.y;

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        if (surrendered) {
            if (pointInRect(returnToTitleButton, mx, my)) {
                surrendered = false;
                menuOpen = false;
                game.setNextState(GameState::Title);
            }
            return;
        }

        if (pointInRect(menuButton, mx, my)) {
            menuOpen = !menuOpen;
            return;
        }

        if (menuOpen && pointInRect(exitGameButton, mx, my)) {
            surrendered = true;
            menuOpen = false;
            drag.active = false;
            return;
        }

        if (menuOpen) menuOpen = false;
    }

    if (surrendered) return;

    // Pending action targeting
    if (pendingAction.active) {
        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
            resolvePendingActionAt(mx, my);
        }
        return;
    }

    // Card dragging logic
    switch (event.type) {
        case SDL_MOUSEBUTTONDOWN:
            for (int i = static_cast<int>(localPlayer.handSize()) - 1; i >= 0; --i) {
                // Compute rects dynamically
                // cardRects[i] corresponds to localPlayer.handInstanceIds[i]
                if (pointInRect(cardRects[i], mx, my)) {
                    drag.active = true;
                    drag.index = static_cast<std::size_t>(i);
                    drag.offsetX = mx - cardRects[drag.index].x;
                    drag.offsetY = my - cardRects[drag.index].y;
                    drag.x = cardRects[drag.index].x;
                    drag.y = cardRects[drag.index].y;
                    break;
                }
            }
            break;

        case SDL_MOUSEMOTION:
            if (drag.active) {
                drag.x = event.motion.x - drag.offsetX;
                drag.y = event.motion.y - drag.offsetY;
            }
            break;

        case SDL_MOUSEBUTTONUP:
            if (drag.active && event.button.button == SDL_BUTTON_LEFT) {
                int laneIndex = -1;
                for (std::size_t slot = 0; slot < playSlots.size(); ++slot) {
                    if (pointInRect(playSlots[slot], mx, my)) {
                        laneIndex = static_cast<int>(slot);
                        break;
                    }
                }

                const bool droppedInDiscard = pointInRect(discardZone, mx, my);

                if (droppedInDiscard) {
                    authority->discardCard(static_cast<int>(drag.index));
                }
                else if (laneIndex >= 0) {
                    // Request server to play card
                    pendingAction.active = true;
                    pendingAction.handIndex = static_cast<int>(drag.index);
                    pendingAction.targetId = std::nullopt; // resolved on click if needed
                }

                drag.active = false;
            }
            break;

        default:
            break;
    }
}

// -------------------------
// Main loop
// -------------------------
void Playing::run() {
    if (!renderer) throw std::runtime_error("Call setup() first");

    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            handleEvents(*reinterpret_cast<Game*>(&event), event);
        }

        animationQueue.update(SDL_GetTicks());
    }
}

void Playing::update(Game&) {
    
    animationQueue.update(SDL_GetTicks());
}

void Playing::render(const Game& game) {
    RenderPlaying::render(*this, game);
}