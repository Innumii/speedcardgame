#include "render/RenderBanner.hpp"
#include "utils/RenderUtil.hpp"

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
    RenderUtil::fillCircle(renderer, s.x + r,       s.y + r,       r);
    RenderUtil::fillCircle(renderer, s.x + s.w - r, s.y + r,       r);
    RenderUtil::fillCircle(renderer, s.x + r,       s.y + s.h - r, r);
    RenderUtil::fillCircle(renderer, s.x + s.w - r, s.y + s.h - r, r);

    // banner body
    RenderUtil::drawRoundedRect(renderer, rect, 14, fill, border);

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
        RenderUtil::drawCenteredText(renderer, font, text, rect, textColor);
    }
}