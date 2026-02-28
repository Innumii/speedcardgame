#include "render/RenderBanner.hpp"
#include <cmath>

static void fillCircle(SDL_Renderer* renderer, int cx, int cy, int r) {
    for (int dy = -r; dy <= r; dy++) {
        int dx = (int)sqrt((double)(r * r - dy * dy));
        SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

static void drawRounded(SDL_Renderer* renderer, const SDL_Rect& rect,
                         SDL_Color fill, SDL_Color border) {
    const int r = 14;

    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
    SDL_Rect body  = {rect.x + r,          rect.y,     rect.w - 2*r, rect.h      };
    SDL_Rect left  = {rect.x,               rect.y + r, r,            rect.h - 2*r};
    SDL_Rect right = {rect.x + rect.w - r,  rect.y + r, r,            rect.h - 2*r};
    SDL_RenderFillRect(renderer, &body);
    SDL_RenderFillRect(renderer, &left);
    SDL_RenderFillRect(renderer, &right);
    fillCircle(renderer, rect.x + r,          rect.y + r,          r);
    fillCircle(renderer, rect.x + rect.w - r, rect.y + r,          r);
    fillCircle(renderer, rect.x + r,          rect.y + rect.h - r, r);
    fillCircle(renderer, rect.x + rect.w - r, rect.y + rect.h - r, r);

    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    SDL_RenderDrawLine(renderer, rect.x + r,      rect.y,          rect.x + rect.w - r, rect.y            );
    SDL_RenderDrawLine(renderer, rect.x + r,      rect.y + rect.h, rect.x + rect.w - r, rect.y + rect.h   );
    SDL_RenderDrawLine(renderer, rect.x,          rect.y + r,      rect.x,              rect.y + rect.h - r);
    SDL_RenderDrawLine(renderer, rect.x + rect.w, rect.y + r,      rect.x + rect.w,     rect.y + rect.h - r);
}

static void drawCentered(SDL_Renderer* renderer, TTF_Font* font,
                          const std::string& text, const SDL_Rect& rect, SDL_Color color) {
    if (!font) return;
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!surface) return;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
        SDL_Rect dst = {
            rect.x + (rect.w - surface->w) / 2,
            rect.y + (rect.h - surface->h) / 2,
            surface->w, surface->h
        };
        SDL_RenderCopy(renderer, texture, nullptr, &dst);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}

void RenderBanner::drawBanner(SDL_Renderer* renderer, const SDL_Rect& rect,
                               const std::string& text, TTF_Font* font,
                               SDL_Color fill, SDL_Color border,
                               SDL_Color textColor, SDL_Color glowColor) {
    if (!renderer) return;

    // shadow
    const int r      = 14;
    const int offset = 5;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 100);
    SDL_Rect s      = {rect.x + offset, rect.y + offset, rect.w, rect.h};
    SDL_Rect sbody  = {s.x + r,         s.y,     s.w - 2*r, s.h        };
    SDL_Rect sleft  = {s.x,             s.y + r, r,         s.h - 2*r  };
    SDL_Rect sright = {s.x + s.w - r,   s.y + r, r,         s.h - 2*r  };
    SDL_RenderFillRect(renderer, &sbody);
    SDL_RenderFillRect(renderer, &sleft);
    SDL_RenderFillRect(renderer, &sright);
    fillCircle(renderer, s.x + r,       s.y + r,       r);
    fillCircle(renderer, s.x + s.w - r, s.y + r,       r);
    fillCircle(renderer, s.x + r,       s.y + s.h - r, r);
    fillCircle(renderer, s.x + s.w - r, s.y + s.h - r, r);

    // banner body
    drawRounded(renderer, rect, fill, border);

    // glow layers then crisp text on top
    if (font) {
        for (int i = 4; i >= 1; i--) {
            Uint8 alpha    = (Uint8)(15 + (4 - i) * 15);
            SDL_Color glow = {glowColor.r, glowColor.g, glowColor.b, alpha};
            SDL_Surface* glowSurface = TTF_RenderUTF8_Blended(font, text.c_str(), glow);
            if (!glowSurface) continue;
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, glowSurface);
            if (t) {
                int cx = rect.x + (rect.w - glowSurface->w) / 2;
                int cy = rect.y + (rect.h - glowSurface->h) / 2;
                for (int dx : {-i, i}) {
                    SDL_Rect dst = {cx + dx, cy, glowSurface->w, glowSurface->h};
                    SDL_RenderCopy(renderer, t, nullptr, &dst);
                }
                for (int dy : {-i, i}) {
                    SDL_Rect dst = {cx, cy + dy, glowSurface->w, glowSurface->h};
                    SDL_RenderCopy(renderer, t, nullptr, &dst);
                }
                SDL_DestroyTexture(t);
            }
            SDL_FreeSurface(glowSurface);
        }
        drawCentered(renderer, font, text, rect, textColor);
    }
}