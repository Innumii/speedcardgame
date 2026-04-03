#include "utils/RenderUtil.hpp"
#include "render/Theme.hpp"

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <unordered_map>
#include <SDL2/SDL_image.h>

namespace {
    std::unordered_map<std::string, SDL_Texture*> gIconCache;

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

// ── Plain (non-rounded) rect helpers ─────────────────────────────────────────

void RenderUtil::drawPlainBorder(SDL_Renderer* renderer, const SDL_Rect& rect,
                                  SDL_Color border, int thickness) {
    if (!renderer || rect.w <= 0 || rect.h <= 0 || thickness <= 0) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);

    const int t = std::max(1, thickness);
    // Top
    SDL_Rect top    = {rect.x,              rect.y,              rect.w, t};
    // Bottom
    SDL_Rect bottom = {rect.x,              rect.y + rect.h - t, rect.w, t};
    // Left (inner edges avoid doubling corners)
    SDL_Rect left   = {rect.x,              rect.y + t,          t, rect.h - 2 * t};
    // Right
    SDL_Rect right  = {rect.x + rect.w - t, rect.y + t,          t, rect.h - 2 * t};

    SDL_RenderFillRect(renderer, &top);
    SDL_RenderFillRect(renderer, &bottom);
    SDL_RenderFillRect(renderer, &left);
    SDL_RenderFillRect(renderer, &right);
}

// Draws a bold L-shaped accent at each of the four corners of `rect`.
//   accentLength    — how far the accent runs along each edge (pixels)
//   accentThickness — how thick the accent arm is (pixels)
void RenderUtil::drawCornerAccents(SDL_Renderer* renderer, const SDL_Rect& rect,
                                    SDL_Color color,
                                    int accentLength, int accentThickness) {
    if (!renderer || rect.w <= 0 || rect.h <= 0) return;

    const int L = std::max(2, std::min(accentLength,    std::min(rect.w, rect.h) / 2));
    const int t = std::max(1, std::min(accentThickness, L));

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    const int x = rect.x;
    const int y = rect.y;
    const int w = rect.w;
    const int h = rect.h;

    // ── Top-left ──────────────────────────────────────────────────────
    SDL_Rect tl_h = {x,         y,     L, t};  // horizontal arm
    SDL_Rect tl_v = {x,         y + t, t, L - t};  // vertical arm (avoids double-drawing corner px)
    // ── Top-right ─────────────────────────────────────────────────────
    SDL_Rect tr_h = {x + w - L, y,     L, t};
    SDL_Rect tr_v = {x + w - t, y + t, t, L - t};
    // ── Bottom-left ───────────────────────────────────────────────────
    SDL_Rect bl_h = {x,         y + h - t,     L, t};
    SDL_Rect bl_v = {x,         y + h - L,     t, L - t};
    // ── Bottom-right ──────────────────────────────────────────────────
    SDL_Rect br_h = {x + w - L, y + h - t,     L, t};
    SDL_Rect br_v = {x + w - t, y + h - L,     t, L - t};

    SDL_RenderFillRect(renderer, &tl_h);
    SDL_RenderFillRect(renderer, &tl_v);
    SDL_RenderFillRect(renderer, &tr_h);
    SDL_RenderFillRect(renderer, &tr_v);
    SDL_RenderFillRect(renderer, &bl_h);
    SDL_RenderFillRect(renderer, &bl_v);
    SDL_RenderFillRect(renderer, &br_h);
    SDL_RenderFillRect(renderer, &br_v);
}

// ─────────────────────────────────────────────────────────────────────────────

void RenderUtil::drawRoundedShadow(SDL_Renderer* renderer, const SDL_Rect& rect, int radius, int offset, SDL_Color color) {
    if (!renderer || rect.w <= 0 || rect.h <= 0) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    SDL_Rect shadowRect{rect.x + offset, rect.y + offset, rect.w, rect.h};
    SDL_Rect body{shadowRect.x + radius, shadowRect.y, shadowRect.w - 2 * radius, shadowRect.h};
    SDL_Rect left{shadowRect.x, shadowRect.y + radius, radius, shadowRect.h - 2 * radius};
    SDL_Rect right{shadowRect.x + shadowRect.w - radius, shadowRect.y + radius, radius, shadowRect.h - 2 * radius};

    SDL_RenderFillRect(renderer, &body);
    SDL_RenderFillRect(renderer, &left);
    SDL_RenderFillRect(renderer, &right);
    fillCircle(renderer, shadowRect.x + radius, shadowRect.y + radius, radius);
    fillCircle(renderer, shadowRect.x + shadowRect.w - radius, shadowRect.y + radius, radius);
    fillCircle(renderer, shadowRect.x + radius, shadowRect.y + shadowRect.h - radius, radius);
    fillCircle(renderer, shadowRect.x + shadowRect.w - radius, shadowRect.y + shadowRect.h - radius, radius);
}

void RenderUtil::drawRoundedGlow(SDL_Renderer* renderer, const SDL_Rect& rect, int radius, SDL_Color glowColor, int layers, Uint8 maxAlpha) {
    if (!renderer || rect.w <= 0 || rect.h <= 0 || layers <= 0 || maxAlpha == 0) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (int i = layers; i >= 1; --i) {
        const Uint8 alpha = static_cast<Uint8>((static_cast<int>(maxAlpha) * (layers - i + 1)) / layers);
        SDL_Color halo{glowColor.r, glowColor.g, glowColor.b, alpha};
        SDL_Rect haloRect{rect.x - i, rect.y - i, rect.w + i * 2, rect.h + i * 2};
        drawRoundedRect(renderer, haloRect, radius, Theme::Effects::CLEAR_COLOR, halo);
    }
}

void RenderUtil::drawRectGlowBorder(SDL_Renderer* renderer, const SDL_Rect& rect, SDL_Color glowColor, int layers, Uint8 baseAlpha, Uint8 alphaStep) {
    if (!renderer || rect.w <= 0 || rect.h <= 0 || layers <= 0) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (int i = layers; i >= 1; --i) {
        const Uint8 alpha = static_cast<Uint8>(baseAlpha + (layers - i) * alphaStep);
        SDL_SetRenderDrawColor(renderer, glowColor.r, glowColor.g, glowColor.b, alpha);
        SDL_Rect glowRect{rect.x - i, rect.y - i, rect.w + i * 2, rect.h + i * 2};
        SDL_RenderDrawRect(renderer, &glowRect);
    }
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

void RenderUtil::drawHexagon(SDL_Renderer* renderer, int centerX, int centerY, int size, SDL_Color fill) {
    if (!renderer || size <= 0) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);

    const double angleStep = M_PI / 3;
    double vx[6], vy[6];
    for (int i = 0; i < 6; ++i) {
        double angle = i * angleStep - M_PI / 2;
        vx[i] = centerX + size * std::cos(angle);
        vy[i] = centerY + size * std::sin(angle);
    }

    int yMin = static_cast<int>(std::ceil(vy[0]));
    int yMax = static_cast<int>(std::floor(vy[3]));

    for (int y = yMin; y <= yMax; ++y) {
        double xLeft  = static_cast<double>(centerX + size);
        double xRight = static_cast<double>(centerX - size);

        for (int i = 0; i < 6; ++i) {
            const int j = (i + 1) % 6;
            const double y0 = vy[i], y1 = vy[j];
            const double x0 = vx[i], x1 = vx[j];

            if ((y < std::min(y0, y1)) || (y > std::max(y0, y1))) continue;
            if (y0 == y1) continue;

            const double t = (y - y0) / (y1 - y0);
            const double xIntersect = x0 + t * (x1 - x0);

            xLeft  = std::min(xLeft,  xIntersect);
            xRight = std::max(xRight, xIntersect);
        }

        SDL_RenderDrawLine(renderer,
            static_cast<int>(std::ceil(xLeft)),
            y,
            static_cast<int>(std::floor(xRight)),
            y
        );
    }
}

SDL_Texture* RenderUtil::getIcon(SDL_Renderer* renderer, const std::string& iconName) {
    if (!renderer || iconName.empty()) return nullptr;

    const auto cacheIt = gIconCache.find(iconName);
    if (cacheIt != gIconCache.end()) {
        return cacheIt->second;
    }

    SDL_Texture* texture = nullptr;
    std::string normalizedName;
    normalizedName.reserve(iconName.size());
    for (char c : iconName) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            normalizedName.push_back(c);
        }
    }

    const std::array<const char*, 3> extensions = {".png", ".jpg", ".bmp"};
    const std::array<std::string, 4> basePaths = {
        "assets/images/" + iconName,
        "assets/images/" + normalizedName,
        "assets/image/" + iconName,
        "assets/image/" + normalizedName
    };

    for (const std::string& basePath : basePaths) {
        for (const char* extension : extensions) {
            const std::string path = basePath + extension;
            SDL_Surface* surface = IMG_Load(path.c_str());
            if (!surface) continue;

            texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_FreeSurface(surface);
            if (texture) break;
        }
        if (texture) break;
    }

    gIconCache[iconName] = texture;
    return texture;
}

void RenderUtil::clearIconCache() {
    for (auto& pair : gIconCache) {
        if (pair.second) {
            SDL_DestroyTexture(pair.second);
        }
    }
    gIconCache.clear();
}

bool RenderUtil::pointInRect(const SDL_Rect& rect, int x, int y) {
    return x >= rect.x && x < rect.x + rect.w &&
           y >= rect.y && y < rect.y + rect.h;
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