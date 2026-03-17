#include "states/Loading.hpp"

#include "core/Game.hpp"
#include "objects/Card.h"
#include "render/RenderBanner.hpp"
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
    currentCardIndex = 0;
    totalCards = 0;
    statusMessage = "Loading cards...";
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
        if (renderer) {
            const auto& availableCards = LoadAvailableCardsUtil::getAvailableCards();
            const Card* card = availableCards[currentCardIndex].get();
            if (card) {
                RenderCard::preloadCardArt(renderer, card->getId());
            }
        }
        ++currentCardIndex;
        return;
    }

    game.setNextState(GameState::Title);
}

void Loading::render(const Game& game) {
    SDL_Renderer* renderer = game.getRenderer();
    if (!renderer) return;

    const RenderText::FontSet& titleFonts = game.getTitleFonts();
    const RenderText::FontSet& uiFonts = game.getUIFonts();

    int screenW = 800;
    int screenH = 600;
    SDL_GetRendererOutputSize(renderer, &screenW, &screenH);

    SDL_SetRenderDrawColor(renderer, Theme::BG.r, Theme::BG.g, Theme::BG.b, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < 80; ++i) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, static_cast<Uint8>(120 - i * 1.5f));
        SDL_Rect edge{i, i, screenW - 2 * i, screenH - 2 * i};
        SDL_RenderDrawRect(renderer, &edge);
    }

    SDL_Rect panel{
        screenW / 2 - 260,
        screenH / 2 - 110,
        520,
        220
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

    const int barMargin = 42;
    SDL_Rect barBg{
        panel.x + barMargin,
        panel.y + panel.h - 78,
        panel.w - (barMargin * 2),
        24
    };

    SDL_SetRenderDrawColor(renderer, 32, 32, 40, 255);
    SDL_RenderFillRect(renderer, &barBg);
    SDL_SetRenderDrawColor(renderer, Theme::BTN_BORDER.r, Theme::BTN_BORDER.g, Theme::BTN_BORDER.b, 255);
    SDL_RenderDrawRect(renderer, &barBg);

    const float progress = totalCards == 0
        ? (cardsPrepared ? 1.0f : 0.0f)
        : static_cast<float>(currentCardIndex) / static_cast<float>(totalCards);
    SDL_Rect fillRect = barBg;
    fillRect.w = static_cast<int>((barBg.w - 2) * std::clamp(progress, 0.0f, 1.0f));
    fillRect.x += 1;
    fillRect.y += 1;
    fillRect.h -= 2;
    SDL_SetRenderDrawColor(renderer, Theme::BTN_START.r, Theme::BTN_START.g, Theme::BTN_START.b, 255);
    SDL_RenderFillRect(renderer, &fillRect);

    if (uiFonts.large) {
        RenderText::drawText(
            renderer,
            statusMessage,
            uiFonts.large,
            Theme::BTN_TEXT,
            panel.x + 42,
            panel.y + 88
        );
    }

    if (uiFonts.small) {
        const std::string progressText = totalCards == 0
            ? "0 / 0"
            : std::to_string(std::min(currentCardIndex, totalCards)) + " / " + std::to_string(totalCards);
        RenderText::drawText(
            renderer,
            progressText,
            uiFonts.small,
            Theme::TEXT_MUTED,
            barBg.x,
            barBg.y - 28
        );
    }
}