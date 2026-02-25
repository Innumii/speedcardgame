#include "states/Playing.hpp"

#include "core/Game.hpp"
#include "render/RenderPLaying.hpp"
#include "render/RenderText.hpp"
#include "objects/Card.h"
#include "objects/CreatureCard.h"
#include "objects/SpellCard.h"
#include "gameplay/GameAuthority.hpp"
#include "gameplay/LocalAuthority.hpp"

#include <SDL2/SDL.h>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    SDL_Rect playZoneBand{0, 0, 0, 0};
}

// -------------------------
// Helpers
// -------------------------
bool Playing::pointInRect(const SDL_Rect& rect, int x, int y) {
    return x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h;
}

bool Playing::isTargetedSpell(const Card& card) const {
    if (card.getType() != CardType::Spell) return false;

    std::string text = card.getText();
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char ch){ return static_cast<char>(std::tolower(ch)); });
    return text.find("target") != std::string::npos;
}

bool Playing::resolvePendingSpellTargetAt(int x, int y) {
    if (!pendingSpellTarget.active || !pendingSpellTarget.spell) return false;

    for (std::size_t lane = 0; lane < playSlots.size(); ++lane) {
        SDL_Rect localRect = playSlots[lane];
        if (pointInRect(localRect, x, y)) {
            authority->castPendingSpell(player.id, lane);
            pendingSpellTarget.active = false;
            return true;
        }

        SDL_Rect opponentRect = playSlots[lane];
        opponentRect.y -= 200;
        int opponentId = player.id == 0 ? 1 : 0;
        if (pointInRect(opponentRect, x, y)) {
            authority->castPendingSpell(opponentId, lane);
            pendingSpellTarget.active = false;
            return true;
        }
    }

    return false;
}

bool Playing::tryDrawCardWithAnimation(Uint32 now) {
    if (!authority || authority->isHandFull(player.id)) return false;

    const std::size_t handSizeBefore = authority->handSize(player.id);
    authority->drawCard(player.id);

    if (authority->handSize(player.id) <= handSizeBefore || !renderer) return false;

    int screenW = 0, screenH = 0;
    if (SDL_GetRendererOutputSize(renderer, &screenW, &screenH) != 0) return false;

    cardRects = computeCardLayout(authority->handSize(player.id), screenW, screenH);
    computeZones(screenW, screenH);

    const SDL_Rect fromRect = computeSelfDeckRect(screenW, screenH);
    const std::size_t handIndex = authority->handSize(player.id) - 1;
    if (handIndex < cardRects.size()) {
        animationQueue.enqueueDrawCard(fromRect, cardRects[handIndex], handIndex, 320);
    }

    return true;
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

void Playing::setup(const Game& game) {
    renderer = game.getRenderer();
    if (!renderer) throw std::runtime_error("Renderer not available");

    drag = DragState{};
    hoverIndex = static_cast<std::size_t>(-1);
    hoverStartTick = 0;
    menuOpen = false;
    surrendered = false;
    animationQueue.clear();
    pendingSpellTarget = PendingSpellTargetState{};
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
        authority = std::make_unique<LocalAuthority>();
    }

    authority->setup(player.id, deck); // initializes player hand, board, etc.
}

// -------------------------
// Event handling
// -------------------------
void Playing::handleEvents(Game& game, const SDL_Event& event) {
    if (!renderer) return;

    int screenW = 0, screenH = 0;
    SDL_GetRendererOutputSize(renderer, &screenW, &screenH);

    cardRects = computeCardLayout(authority->handSize(player.id), screenW, screenH);
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

    // Pending spell targeting
    if (pendingSpellTarget.active) {
        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
            resolvePendingSpellTargetAt(mx, my);
        }
        return;
    }

    // Card dragging logic
    switch (event.type) {
        case SDL_MOUSEBUTTONDOWN:
            for (int i = static_cast<int>(cardRects.size()) - 1; i >= 0; --i) {
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

                authority->handleCardDrop(player.id, drag.index, laneIndex, droppedInDiscard, pendingSpellTarget);

                drag.active = false;
                cardRects = computeCardLayout(authority->handSize(player.id), screenW, screenH);
                computeZones(screenW, screenH);
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
            handleEvents(*authority, event);
        }

        const Uint32 now = SDL_GetTicks();
        animationQueue.update(now);

        if (now - lastDrawTick >= static_cast<Uint32>(drawIntervalSeconds * 1000)) {
            tryDrawCardWithAnimation(now);
            lastDrawTick = now;
        }
    }
}

void Playing::update(Game&) {
    const Uint32 now = SDL_GetTicks();
    animationQueue.update(now);
    if (now - lastDrawTick >= static_cast<Uint32>(drawIntervalSeconds * 1000)) {
        tryDrawCardWithAnimation(now);
        lastDrawTick = now;
    }
}

void Playing::render(const Game& game) {
    RenderPlaying::render(*this, game);
}
