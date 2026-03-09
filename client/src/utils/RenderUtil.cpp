#include "utils/RenderUtil.hpp"

#include <algorithm>
#include <cmath>

namespace {
    int clampRadius(const SDL_Rect& rect, int radius) {
        return std::max(0, std::min(radius, std::min(rect.w, rect.h) / 2));
    }
}

void RenderUtil::fillCircle(SDL_Renderer* renderer, int centerX, int centerY, int radius) {
    if (!renderer || radius <= 0) return;

    for (int dy = -radius; dy <= radius; ++dy) {
        const int dx = static_cast<int>(std::sqrt(static_cast<double>(radius * radius - dy * dy)));
        SDL_RenderDrawLine(renderer, centerX - dx, centerY + dy, centerX + dx, centerY + dy);
    }
}

void RenderUtil::fillRoundedRect(SDL_Renderer* renderer, const SDL_Rect& rect, int radius, SDL_Color fill) {
    if (!renderer || rect.w <= 0 || rect.h <= 0) return;

    const int r = clampRadius(rect, radius);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);

    SDL_Rect body = {rect.x + r, rect.y, rect.w - 2 * r, rect.h};
    SDL_Rect left = {rect.x, rect.y + r, r, rect.h - 2 * r};
    SDL_Rect right = {rect.x + rect.w - r, rect.y + r, r, rect.h - 2 * r};

    SDL_RenderFillRect(renderer, &body);
    SDL_RenderFillRect(renderer, &left);
    SDL_RenderFillRect(renderer, &right);

    fillCircle(renderer, rect.x + r, rect.y + r, r);
    fillCircle(renderer, rect.x + rect.w - r, rect.y + r, r);
    fillCircle(renderer, rect.x + r, rect.y + rect.h - r, r);
    fillCircle(renderer, rect.x + rect.w - r, rect.y + rect.h - r, r);
}

void RenderUtil::drawRoundedBorder(SDL_Renderer* renderer, const SDL_Rect& rect, int radius, SDL_Color border, int thickness) {
    if (!renderer || rect.w <= 0 || rect.h <= 0 || thickness <= 0) return;

    const int r = clampRadius(rect, radius);
    const int t = std::max(1, std::min(thickness, r));

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);

    const int outerR = r;
    const int innerR = outerR - t;

    SDL_Rect top = {rect.x + outerR, rect.y, rect.w - 2 * outerR, t};
    SDL_Rect bottom = {rect.x + outerR, rect.y + rect.h - t, rect.w - 2 * outerR, t};
    SDL_Rect left = {rect.x, rect.y + outerR, t, rect.h - 2 * outerR};
    SDL_Rect right = {rect.x + rect.w - t, rect.y + outerR, t, rect.h - 2 * outerR};

    SDL_RenderFillRect(renderer, &top);
    SDL_RenderFillRect(renderer, &bottom);
    SDL_RenderFillRect(renderer, &left);
    SDL_RenderFillRect(renderer, &right);

    for (int dy = 0; dy <= outerR; ++dy) {
        const int outerDx = static_cast<int>(std::sqrt(std::max(0.0, static_cast<double>(outerR * outerR - dy * dy))));
        const int innerDx = (innerR > 0 && dy < innerR)
            ? static_cast<int>(std::sqrt(std::max(0.0, static_cast<double>(innerR * innerR - dy * dy))))
            : 0;

        const int topRow = rect.y + outerR - dy;
        SDL_RenderDrawLine(renderer, rect.x + outerR - outerDx, topRow, rect.x + outerR - innerDx, topRow);
        SDL_RenderDrawLine(renderer, rect.x + rect.w - outerR + innerDx, topRow, rect.x + rect.w - outerR + outerDx, topRow);

        const int bottomRow = rect.y + rect.h - outerR + dy;
        SDL_RenderDrawLine(renderer, rect.x + outerR - outerDx, bottomRow, rect.x + outerR - innerDx, bottomRow);
        SDL_RenderDrawLine(renderer, rect.x + rect.w - outerR + innerDx, bottomRow, rect.x + rect.w - outerR + outerDx, bottomRow);
    }
}

void RenderUtil::drawRoundedRect(SDL_Renderer* renderer, const SDL_Rect& rect, int radius, SDL_Color fill, SDL_Color border) {
    fillRoundedRect(renderer, rect, radius, fill);
    drawRoundedBorder(renderer, rect, radius, border, 1);
}

void RenderUtil::drawCenteredText(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, const SDL_Rect& rect, SDL_Color color) {
    if (!renderer || !font || text.empty()) return;

    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!surface) return;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
        SDL_Rect dst = {
            rect.x + (rect.w - surface->w) / 2,
            rect.y + (rect.h - surface->h) / 2,
            surface->w,
            surface->h
        };
        SDL_RenderCopy(renderer, texture, nullptr, &dst);
        SDL_DestroyTexture(texture);
    }

    SDL_FreeSurface(surface);
}

SDL_Color RenderUtil::brighten(SDL_Color color, int amount) {
    return {
        static_cast<Uint8>(std::min(static_cast<int>(color.r) + amount, 255)),
        static_cast<Uint8>(std::min(static_cast<int>(color.g) + amount, 255)),
        static_cast<Uint8>(std::min(static_cast<int>(color.b) + amount, 255)),
        color.a
    };
}

SDL_Color RenderUtil::darken(SDL_Color color, int amount) {
    return {
        static_cast<Uint8>(std::max(static_cast<int>(color.r) - amount, 0)),
        static_cast<Uint8>(std::max(static_cast<int>(color.g) - amount, 0)),
        static_cast<Uint8>(std::max(static_cast<int>(color.b) - amount, 0)),
        color.a
    };
}
