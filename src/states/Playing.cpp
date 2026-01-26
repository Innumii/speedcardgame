#include "states/Playing.hpp"

#include "core/Game.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "objects/CreatureCard.h"
#include "objects/SpellCard.h"
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

    drawText(
        "Health: " + std::to_string(player.health),
        fontLarge.get(),
        SDL_Color{255, 255, 255, 255},
        20,
        20
    );

    const int cardWidth = 120;
    const int cardHeight = 180;
    const int spacing = 20;

    for (std::size_t i = 0; i < player.hand.size(); i++) {
        const auto& cardPtr = player.hand[i];
        const Card* card = cardPtr.get();
        if (!card) continue;

        SDL_Rect cardRect{
            50 + static_cast<int>(i) * (cardWidth + spacing),
            350,
            cardWidth,
            cardHeight
        };

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
    }

    SDL_RenderPresent(renderer);
}
