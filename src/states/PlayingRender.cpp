#include "states/Playing.hpp"

#include "core/Game.hpp"
#include "objects/CreatureCard.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <iostream>
#include <sstream>
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

void Playing::render(Game& game) {
    (void)game; // renderer does not need the game reference yet

    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
    SDL_RenderClear(renderer);

    const Uint32 now = SDL_GetTicks();

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
    computeZones(screenW, screenH);

    int mouseX = 0, mouseY = 0;
    SDL_GetMouseState(&mouseX, &mouseY);
    const bool hoveringDiscard = pointInRect(discardZone, mouseX, mouseY);

    const bool draggingCard = drag.active && drag.index < player.hand.size();

    std::size_t newHoverIndex = static_cast<std::size_t>(-1);
    if (!draggingCard) {
        for (std::size_t i = 0; i < cardRects.size(); ++i) {
            if (pointInRect(cardRects[i], mouseX, mouseY)) {
                newHoverIndex = i;
                break;
            }
        }
    }

    if (newHoverIndex != hoverIndex) {
        hoverIndex = newHoverIndex;
        hoverStartTick = now;
    }

    constexpr Uint32 hoverDelayMs = 1000;
    const bool showPreview =
        hoverIndex != static_cast<std::size_t>(-1) &&
        hoverIndex < player.hand.size() &&
        now - hoverStartTick >= hoverDelayMs;

    if (!playSlots.empty()) {
        const int areaX = playSlots.front().x;
        const int areaY = playSlots.front().y;

        const int labelY = std::max(10, areaY - 18);
        drawText(
            "Play Zone",
            fontSmall.get(),
            SDL_Color{210, 230, 210, 255},
            areaX,
            labelY
        );

        for (const auto& slot : playSlots) {
            SDL_SetRenderDrawColor(renderer, 60, 80, 60, 190);
            SDL_RenderFillRect(renderer, &slot);
            SDL_SetRenderDrawColor(renderer, 140, 190, 140, 255);
            SDL_RenderDrawRect(renderer, &slot);
        }
    }

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

    auto drawCardAtPtr = [&](const Card* card, const SDL_Rect& cardRect) {
        if (!card) return;

        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        SDL_RenderFillRect(renderer, &cardRect);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderDrawRect(renderer, &cardRect);

        const int textMargin = 10;
        drawText(card->getName(), fontSmall.get(), SDL_Color{0, 0, 0, 255}, cardRect.x + textMargin, cardRect.y + textMargin);

        drawText("Cost: " + std::to_string(card->getManaCost()), fontSmall.get(),
                SDL_Color{0,0,0,255}, cardRect.x + cardRect.w - 40, cardRect.y + textMargin);

        drawText("Value: " + std::to_string(card->getManaValue()), fontSmall.get(),
                SDL_Color{0,0,0,255}, cardRect.x + textMargin, cardRect.y + textMargin + 22);
    };


    auto drawCardPreview = [&](const Card& card) {
        const int previewWidth = 260;
        const int previewHeight = 240;
        const int padding = 12;

        int handY = screenH - 165 - 30;
        if (!cardRects.empty()) {
            handY = cardRects.front().y;
        }

        int previewY = handY - previewHeight - 10;
        if (previewY < 20) previewY = 20;
        const int previewX = 20;

        SDL_Rect panel{previewX, previewY, previewWidth, previewHeight};
        SDL_SetRenderDrawColor(renderer, 45, 45, 60, 245);
        SDL_RenderFillRect(renderer, &panel);
        SDL_SetRenderDrawColor(renderer, 220, 220, 240, 255);
        SDL_RenderDrawRect(renderer, &panel);

        drawText(card.getName(), fontLarge.get(), SDL_Color{255, 255, 255, 255}, previewX + padding, previewY + padding);

        const std::string costText = "Cost: " + std::to_string(card.getManaCost());
        int costW = 0, costH = 0;
        TTF_SizeText(fontSmall.get(), costText.c_str(), &costW, &costH);
        drawText(costText, fontSmall.get(), SDL_Color{230, 230, 230, 255}, previewX + previewWidth - padding - costW, previewY + padding);

        drawText(
            "Value: " + std::to_string(card.getManaValue()),
            fontSmall.get(),
            SDL_Color{230, 230, 230, 255},
            previewX + padding,
            previewY + padding + 28
        );

        const int descY = previewY + padding + 52;
        const int wrapWidth = std::max(16, (previewWidth - 2 * padding) / 7);
        drawWrappedText(
            card.getText(),
            fontSmall.get(),
            SDL_Color{220, 220, 220, 255},
            previewX + padding,
            descY,
            static_cast<std::size_t>(wrapWidth)
        );

        if (card.getType() == CardType::Creature) {
            const auto* creature = dynamic_cast<const CreatureCard*>(&card);
            if (creature) {
                const std::string statsText =
                    std::to_string(creature->getPower()) + "/" +
                    std::to_string(creature->getToughness());

                int statsW = 0, statsH = 0;
                TTF_SizeText(fontSmall.get(), statsText.c_str(), &statsW, &statsH);

                drawText(
                    statsText,
                    fontSmall.get(),
                    SDL_Color{230, 230, 230, 255},
                    previewX + previewWidth - padding - statsW,
                    previewY + previewHeight - padding - statsH
                );
            }
        }
    };

    for (std::size_t i = 0; i < player.hand.size(); ++i) {
        if (draggingCard && i == drag.index) continue;
        if (i < cardRects.size()) {
            drawCardAt(i, cardRects[i]);
        }
    }

    for (int pid = 0; pid <= 1; ++pid) {
        for (size_t lane = 0; lane < board.getLaneCount(); ++lane) {
            const auto& optCard = board.getZone(static_cast<int>(lane), pid);
            if (optCard && *optCard) {
                const Card* card = optCard->get();

                SDL_Rect rect = playSlots[lane];
                if (pid == 1) {
                    rect.y -= 50;
                }

                drawCardAtPtr(card, rect);
            }
        }
    }

    if (draggingCard && drag.index < cardRects.size()) {
        SDL_Rect floating = cardRects[drag.index];
        floating.x = drag.x;
        floating.y = drag.y;
        drawCardAt(drag.index, floating);
    }

    if (showPreview) {
        if (const auto& cardPtr = player.hand[hoverIndex]) {
            drawCardPreview(*cardPtr);
        }
    }

    SDL_RenderPresent(renderer);
}
