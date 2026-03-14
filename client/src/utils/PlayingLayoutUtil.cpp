#include "utils/PlayingLayoutUtil.hpp"

#include <algorithm>

namespace PlayingLayoutUtil {
    SDL_Rect computeDeckRect(const std::vector<SDL_Rect>& slots, int screenW,
                             int width, int height, int gap, int margin) {
        if (slots.empty() || screenW <= 0 || width <= 0 || height <= 0) {
            return SDL_Rect{0, 0, 0, 0};
        }

        const int deckY = slots.front().y + (slots.front().h - height) / 2;
        int deckX = slots.back().x + slots.back().w + gap;
        if (deckX + width > screenW - margin) {
            deckX = std::max(margin, screenW - margin - width);
        }

        return SDL_Rect{deckX, deckY, width, height};
    }

    SDL_Rect computeDiscardRect(const std::vector<SDL_Rect>& slots,
                                int width, int height, int gap, int margin) {
        if (slots.empty() || width <= 0 || height <= 0) {
            return SDL_Rect{0, 0, 0, 0};
        }

        int discardX = slots.front().x - gap - width;
        if (discardX < margin) {
            discardX = margin;
        }

        const int discardY = slots.front().y + (slots.front().h - height) / 2;
        return SDL_Rect{discardX, discardY, width, height};
    }

    int computeOpponentHandY(const std::vector<SDL_Rect>& opponentSlots,
                             int cardHeight, int topMargin) {
        if (opponentSlots.empty()) {
            return topMargin;
        }

        return std::max(topMargin, opponentSlots.front().y - cardHeight - topMargin);
    }
}