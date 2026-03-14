#ifndef PLAYING_LAYOUT_UTIL_HPP
#define PLAYING_LAYOUT_UTIL_HPP

#include <SDL2/SDL.h>
#include <vector>

namespace PlayingLayoutUtil {
    SDL_Rect computeDeckRect(const std::vector<SDL_Rect>& slots, int screenW,
                             int width, int height, int gap, int margin);

    SDL_Rect computeDiscardRect(const std::vector<SDL_Rect>& slots,
                                int width, int height, int gap, int margin);

    int computeOpponentHandY(const std::vector<SDL_Rect>& opponentSlots,
                             int cardHeight, int topMargin);
}

#endif