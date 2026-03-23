#include "states/Loading.hpp"

#include "core/Game.hpp"
#include "objects/Card.h"
#include "render/RenderBanner.hpp"
#include "render/RenderBackdrop.hpp"
#include "render/RenderCard.hpp"
#include "render/RenderText.hpp"
#include "render/Theme.hpp"
#include "utils/LoadAvailableCards.hpp"

#include <SDL2/SDL.h>
#include <algorithm>
#include <string>

void Loading::enter(Game& game) {
    (void)game;
    cardsPrepared = false;
    deckPrepared = false;
    retryRounds = 0;
    currentCardIndex = 0;
    totalCards = 0;
    statusMessage = "Loading assets...";
}

void Loading::exit(Game& game) {
    (void)game;
}

void Loading::handleEvents(Game& game, const SDL_Event& event) {
    if (event.type == SDL_QUIT) {
        game.setNextState(GameState::Quit);
    }
}

void Loading::update(Game& game) {
    if (!cardsPrepared) {
        cardsPrepared = true;
        LoadAvailableCardsUtil::ensureAvailableCardsLoaded();
        totalCards = LoadAvailableCardsUtil::getAvailableCards().size();
        statusMessage = totalCards > 0 ? "Caching card art..." : "No cards found.";
    }

    if (!deckPrepared) {
        deckPrepared = true;
        if (!game.refreshPlayerDeckFromService()) {
            statusMessage = totalCards > 0 ? "Caching card art..." : "Continuing without deck data.";
        }
    }

    if (currentCardIndex < totalCards) {
        SDL_Renderer* renderer = game.getRenderer();
        if (!renderer) {
            statusMessage = "Waiting for renderer...";
            return;
        }

        const auto& availableCards = LoadAvailableCardsUtil::getAvailableCards();
        const Card* card = availableCards[currentCardIndex].get();
        if (card) {
            (void)RenderCard::preloadCardArt(renderer, card->getId());
        }

        ++currentCardIndex;
        return;
    }

    if (totalCards > 0) {
        const auto& availableCards = LoadAvailableCardsUtil::getAvailableCards();
        std::size_t cachedCount = 0;
        for (const auto& cardPtr : availableCards) {
            const Card* card = cardPtr.get();
            if (card && RenderCard::isCardArtCached(card->getId())) {
                ++cachedCount;
            }
        }

        if (cachedCount < totalCards) {

            if (retryRounds >= maxRetryRounds) {
                statusMessage = "Failed to download, return to login";
                game.setNextState(GameState::Login);
            } else {
                ++retryRounds;
                currentCardIndex = 0;
                statusMessage = "Caching card art... (" + std::to_string(cachedCount) + "/" +
                    std::to_string(totalCards) + ", retry " + std::to_string(retryRounds) + "/" + std::to_string(maxRetryRounds) + ")";
                return;
            }
        }
    }

    game.setNextState(GameState::Title);
}

void Loading::render(const Game& game) {
    SDL_Renderer* renderer = game.getRenderer();
    if (!renderer) return;

    const RenderText::FontSet& titleFonts = game.getTitleFonts();
    const RenderText::FontSet& uiFonts = game.getUIFonts();

    int screenW = 800, screenH = 600;
    SDL_GetRendererOutputSize(renderer, &screenW, &screenH);

    // ✅ SAME scaling system as Login
    constexpr float kRefW = 1200.0f;
    constexpr float kRefH = 850.0f;

    const float scale = std::min(
        static_cast<float>(screenW) / kRefW,
        static_cast<float>(screenH) / kRefH
    );

    // ── background ───────────────────────────────────────────────
    RenderBackdrop::drawBackgroundWithVignette(
        renderer,
        screenW,
        screenH,
        Theme::BG,
        Theme::Loading::VIGNETTE_COLOR,
        Theme::Loading::VIGNETTE_LAYERS,
        Theme::Loading::VIGNETTE_ALPHA_FALLOFF,
        Theme::Loading::VIGNETTE_MAX_ALPHA
    );

    // ── scaled panel ─────────────────────────────────────────────
    const int panelW = static_cast<int>(Theme::Loading::PANEL_WIDTH * scale);
    const int panelH = static_cast<int>(Theme::Loading::PANEL_HEIGHT * scale);
    const int panelOffsetY = static_cast<int>(Theme::Loading::PANEL_OFFSET_Y * scale);

    SDL_Rect panel{
        screenW / 2 - panelW / 2,
        screenH / 2 - panelOffsetY,
        panelW,
        panelH
    };

    RenderBanner::drawBanner(
        renderer,
        panel,
        "Preparing Cards",
        titleFonts.medium ? titleFonts.medium : titleFonts.large,
        Theme::BANNER_FILL,
        Theme::BANNER_BORDER,
        Theme::BANNER_TEXT,
        Theme::BANNER_GLOW
    );

    // ── progress bar ─────────────────────────────────────────────
    const int barMargin = static_cast<int>(Theme::Loading::BAR_MARGIN_X * scale);
    const int barHeight = static_cast<int>(Theme::Loading::BAR_HEIGHT * scale);
    const int barOffsetBottom = static_cast<int>(Theme::Loading::BAR_OFFSET_FROM_BOTTOM * scale);

    SDL_Rect barBg{
        panel.x + barMargin,
        panel.y + panel.h - barOffsetBottom,
        panel.w - (barMargin * 2),
        barHeight
    };

    SDL_SetRenderDrawColor(renderer,
        Theme::Loading::BAR_BACKGROUND.r,
        Theme::Loading::BAR_BACKGROUND.g,
        Theme::Loading::BAR_BACKGROUND.b,
        Theme::Loading::BAR_BACKGROUND.a);
    SDL_RenderFillRect(renderer, &barBg);

    SDL_SetRenderDrawColor(renderer,
        Theme::BTN_BORDER.r,
        Theme::BTN_BORDER.g,
        Theme::BTN_BORDER.b,
        255);
    SDL_RenderDrawRect(renderer, &barBg);

    // ── progress fill ────────────────────────────────────────────
    const float progress = totalCards == 0
        ? (cardsPrepared ? 1.0f : 0.0f)
        : static_cast<float>(currentCardIndex) / static_cast<float>(totalCards);

    const int inset = static_cast<int>(Theme::Loading::BAR_INNER_INSET * scale);
    const int heightReduction = static_cast<int>(Theme::Loading::BAR_INNER_HEIGHT_REDUCTION * scale);

    SDL_Rect fillRect = barBg;
    fillRect.x += inset;
    fillRect.y += inset;
    fillRect.h -= heightReduction;

    fillRect.w = static_cast<int>(
        (barBg.w - inset * 2) * std::clamp(progress, 0.0f, 1.0f)
    );

    SDL_SetRenderDrawColor(renderer,
        Theme::BTN_START.r,
        Theme::BTN_START.g,
        Theme::BTN_START.b,
        255);
    SDL_RenderFillRect(renderer, &fillRect);

    // ── status text ──────────────────────────────────────────────
    if (uiFonts.large) {
        const int textOffsetX = static_cast<int>(Theme::Loading::STATUS_TEXT_OFFSET_X * scale);
        const int textOffsetY = static_cast<int>(Theme::Loading::STATUS_TEXT_OFFSET_Y * scale);

        RenderText::drawText(
            renderer,
            statusMessage,
            uiFonts.large,
            Theme::BTN_TEXT,
            panel.x + textOffsetX,
            panel.y + textOffsetY
        );
    }
}