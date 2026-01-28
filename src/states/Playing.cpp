#include "states/Playing.hpp"

#include "core/Game.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "objects/CreatureCard.h"
#include "objects/SpellCard.h"
#include <algorithm>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    std::vector<std::string> wrapWords(const std::string& text, std::size_t maxLineLen) {
        std::vector<std::string> lines;
        std::istringstream words(text);
        std::string word;
        std::string current;

        while (words >> word) {
            const bool fitsOnLine = current.size() + (current.empty() ? 0 : 1) + word.size() <= maxLineLen;
            if (!current.empty() && !fitsOnLine) {
                lines.push_back(current);
                current.clear();
            }

            if (!current.empty()) current.push_back(' ');
            current.append(word);
        }

        if (!current.empty()) {
            lines.push_back(current);
        }

        return lines;
    }

    bool pointInRect(const SDL_Rect& rect, int x, int y) {
        return x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h;
    }
}

Playing::Playing(int drawIntervalSeconds)
    : drawIntervalSeconds(drawIntervalSeconds) {}

Playing::~Playing() {
    fontSmall.reset();
    fontLarge.reset();
    if (TTF_WasInit()) {
        TTF_Quit();
    }
}

void Playing::setup(Game& game) {
    renderer = game.getRenderer();
    if (!renderer) {
        throw std::runtime_error("Renderer not available from Game");
    }

    if (!TTF_WasInit() && TTF_Init() != 0) {
        throw std::runtime_error(std::string("TTF_Init failed: ") + TTF_GetError());
    }

    fontLarge.reset(TTF_OpenFont("assets/font.ttf", 24));
    if (!fontLarge) {
        throw std::runtime_error(std::string("Failed to load font (24pt): ") + TTF_GetError());
    }

    fontSmall.reset(TTF_OpenFont("assets/font.ttf", 14));
    if (!fontSmall) {
        fontLarge.reset();
        throw std::runtime_error(std::string("Failed to load font (14pt): ") + TTF_GetError());
    }

    for (int i = 0; i < 20; i++) {
        deck.addCard(std::make_unique<CreatureCard>(
            "Goblin",
            "A small but angry creature",
            1, 1, 1, 1
        ));

        deck.addCard(std::make_unique<SpellCard>(
            "Fireball",
            "Deal 3 damage",
            2, 2
        ));
    }

    deck.shuffle();
    lastDrawTick = SDL_GetTicks();
    running = true;
}

void Playing::drawText(const std::string& text, TTF_Font* font, SDL_Color color, int x, int y) {
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!surface) {
        std::cerr << "TTF_RenderUTF8_Blended failed: " << TTF_GetError() << '\n';
        return;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        std::cerr << "SDL_CreateTextureFromSurface failed: " << SDL_GetError() << '\n';
        SDL_FreeSurface(surface);
        return;
    }

    SDL_Rect dst{x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, nullptr, &dst);

    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

void Playing::drawWrappedText(const std::string& text, TTF_Font* font, SDL_Color color, int x, int y, std::size_t maxLineLen) {
    const auto lines = wrapWords(text, maxLineLen);
    int lineSkip = TTF_FontLineSkip(font);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        drawText(lines[i], font, color, x, y + static_cast<int>(i) * lineSkip);
    }
}

std::vector<SDL_Rect> Playing::computeCardLayout(std::size_t count, int screenW, int screenH) const {
    std::vector<SDL_Rect> layout;
    if (count == 0 || screenW <= 0 || screenH <= 0) return layout;

    const int cardWidth = 110;
    const int cardHeight = 165;
    const int maxWidth = static_cast<int>(screenW * 0.8f); // Use 80% of screen width
    
    // 1. Calculate how much space we need if cards didn't overlap
    int totalWidthNoOverlap = static_cast<int>(count) * cardWidth;
    
    // 2. Determine spacing. 
    // If totalWidth > maxWidth, spacing becomes negative (overlap).
    int spacing = 10; // Default gap between cards
    if (totalWidthNoOverlap > maxWidth && count > 1) {
        spacing = (maxWidth - totalWidthNoOverlap) / static_cast<int>(count - 1);
    }

    // 3. Calculate actual total width with the new spacing
    int finalHandWidth = (static_cast<int>(count) * cardWidth) + (static_cast<int>(count - 1) * spacing);
    
    // 4. Center the start position
    int startX = (screenW - finalHandWidth) / 2;
    int startY = screenH - cardHeight - 30; // Fixed distance from bottom

    for (std::size_t i = 0; i < count; ++i) {
        layout.push_back(SDL_Rect{
            startX + static_cast<int>(i) * (cardWidth + spacing),
            startY,
            cardWidth,
            cardHeight
        });
    }

    return layout;
}

SDL_Rect Playing::computeDiscardZone(int screenW, int screenH) const {
    const int zoneWidth = 180;
    const int zoneHeight = 120;
    const int margin = 20;

    const int cardHeight = 165;
    const int handY = screenH - cardHeight - 30;

    const int x = screenW - zoneWidth - margin;
    const int y = std::max(margin, handY - zoneHeight - 12);

    return SDL_Rect{x, y, zoneWidth, zoneHeight};
}

void Playing::handleEvent(Game& /*game*/, const SDL_Event& event) {
    if (!renderer) return;

    int screenW = 0, screenH = 0;
    if (SDL_GetRendererOutputSize(renderer, &screenW, &screenH) != 0) {
        return;
    }

    cardRects = computeCardLayout(player.hand.size(), screenW, screenH);
    discardZone = computeDiscardZone(screenW, screenH);

    switch (event.type) {
        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_LEFT) {
                const int mx = event.button.x;
                const int my = event.button.y;
                for (int i = static_cast<int>(cardRects.size()) - 1; i >= 0; --i) {
                    if (pointInRect(cardRects[static_cast<std::size_t>(i)], mx, my)) {
                        drag.active = true;
                        drag.index = static_cast<std::size_t>(i);
                        drag.offsetX = mx - cardRects[drag.index].x;
                        drag.offsetY = my - cardRects[drag.index].y;
                        drag.x = cardRects[drag.index].x;
                        drag.y = cardRects[drag.index].y;
                        break;
                    }
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
                const int releaseX = event.button.x;
                const int releaseY = event.button.y;

                const bool droppedInDiscard =
                    drag.index < player.hand.size() &&
                    pointInRect(discardZone, releaseX, releaseY);

                if (droppedInDiscard) {
                    const auto manaGain = player.hand[drag.index]->getManaValue();
                    player.addMana(manaGain);
                    player.hand.erase(player.hand.begin() + static_cast<std::ptrdiff_t>(drag.index));
                    cardRects = computeCardLayout(player.hand.size(), screenW, screenH);
                    discardZone = computeDiscardZone(screenW, screenH);
                }

                drag.active = false;
            }
            break;
        default:
            break;
    }
}

void Playing::run() {
    if (!renderer) {
        throw std::runtime_error("Call setup() before run()");
    }

    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        const Uint32 now = SDL_GetTicks();
        if (now - lastDrawTick >= static_cast<Uint32>(drawIntervalSeconds * 1000)) {
            if (!player.handFull()) {
                player.drawCard(deck);
            }
            lastDrawTick = now;
        }
    }
}

void Playing::update(Game& /*game*/) {
    if (!renderer) return; // not yet ready

    const Uint32 now = SDL_GetTicks();
    if (now - lastDrawTick >= static_cast<Uint32>(drawIntervalSeconds * 1000)) {
        if (!player.handFull()) {
            player.drawCard(deck);
        }
        lastDrawTick = now;
    }
}

void Playing::render(Game& game) {
    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
    SDL_RenderClear(renderer);

    int screenW = 0, screenH = 0;
    if (SDL_GetRendererOutputSize(renderer, &screenW, &screenH) != 0) {
        screenW = 800;
        screenH = 600;
    }

    drawText(
        "Health: " + std::to_string(player.health),
        fontLarge.get(),
        SDL_Color{255, 255, 255, 255},
        20,
        20
    );

    const std::string manaText = "Mana: " + std::to_string(player.mana);
    int manaW = 0, manaH = 0;
    if (fontLarge) {
        TTF_SizeText(fontLarge.get(), manaText.c_str(), &manaW, &manaH);
    }
    drawText(
        manaText,
        fontLarge.get(),
        SDL_Color{255, 255, 255, 255},
        screenW - manaW - 20,
        20
    );

    cardRects = computeCardLayout(player.hand.size(), screenW, screenH);
    discardZone = computeDiscardZone(screenW, screenH);

    int mouseX = 0, mouseY = 0;
    SDL_GetMouseState(&mouseX, &mouseY);
    const bool hoveringDiscard = pointInRect(discardZone, mouseX, mouseY);

    SDL_SetRenderDrawColor(renderer, hoveringDiscard ? 80 : 60, 80, 110, 255);
    SDL_RenderFillRect(renderer, &discardZone);
    SDL_SetRenderDrawColor(renderer, 190, 190, 220, 255);
    SDL_RenderDrawRect(renderer, &discardZone);

    drawText(
        "Discard Zone",
        fontSmall.get(),
        SDL_Color{255, 255, 255, 255},
        discardZone.x + 10,
        discardZone.y + 10
    );

    drawWrappedText(
        "Drop cards here to gain mana",
        fontSmall.get(),
        SDL_Color{220, 220, 220, 255},
        discardZone.x + 10,
        discardZone.y + 32,
        20
    );

    auto drawCardAt = [&](std::size_t idx, const SDL_Rect& cardRect) {
        const auto& cardPtr = player.hand[idx];
        const Card* card = cardPtr.get();
        if (!card) return;

        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        SDL_RenderFillRect(renderer, &cardRect);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderDrawRect(renderer, &cardRect);

        const int textMargin = 10;
        drawText(card->getName(), fontSmall.get(), SDL_Color{0, 0, 0, 255}, cardRect.x + textMargin, cardRect.y + textMargin);

        const std::string costText = "Cost: " + std::to_string(card->getManaCost());
        int costW = 0, costH = 0;
        TTF_SizeText(fontSmall.get(), costText.c_str(), &costW, &costH);
        drawText(costText, fontSmall.get(), SDL_Color{0, 0, 0, 255}, cardRect.x + cardRect.w - costW - textMargin, cardRect.y + textMargin);

        drawText(
            "Value: " + std::to_string(card->getManaValue()),
            fontSmall.get(),
            SDL_Color{0, 0, 0, 255},
            cardRect.x + textMargin,
            cardRect.y + textMargin + 22
        );

        drawWrappedText(
            card->getText(),
            fontSmall.get(),
            SDL_Color{0, 0, 0, 255},
            cardRect.x + textMargin,
            cardRect.y + textMargin + 48,
            18
        );

        if (card->getType() == CardType::Creature) {
            const auto* creature = dynamic_cast<const CreatureCard*>(card);
            if (creature) {
                const std::string statsText =
                    std::to_string(creature->getPower()) + "/" +
                    std::to_string(creature->getToughness());

                int statsW = 0, statsH = 0;
                TTF_SizeText(fontSmall.get(), statsText.c_str(), &statsW, &statsH);

                drawText(
                    statsText,
                    fontSmall.get(),
                    SDL_Color{0, 0, 0, 255},
                    cardRect.x + cardRect.w - statsW - 12,
                    cardRect.y + cardRect.h - statsH - 12
                );
            }
        }
    };

    const bool draggingCard = drag.active && drag.index < player.hand.size();

    for (std::size_t i = 0; i < player.hand.size(); ++i) {
        if (draggingCard && i == drag.index) continue;
        if (i < cardRects.size()) {
            drawCardAt(i, cardRects[i]);
        }
    }

    if (draggingCard && drag.index < cardRects.size()) {
        SDL_Rect floating = cardRects[drag.index];
        floating.x = drag.x;
        floating.y = drag.y;
        drawCardAt(drag.index, floating);
    }

    SDL_RenderPresent(renderer);
}
