#include "render/RenderBanner.hpp"
#include "render/Theme.hpp"
#include "utils/RenderUtil.hpp"

void RenderBanner::drawBanner(SDL_Renderer* renderer, const SDL_Rect& rect,
                               const std::string& text, TTF_Font* font,
                               SDL_Color fill, SDL_Color border,
                               SDL_Color textColor, SDL_Color glowColor) {
    if (!renderer) return;

    // shadow
    const int r = Theme::BTN_RADIUS;
    RenderUtil::drawRoundedShadow(renderer, rect, r, Theme::Effects::SHADOW_OFFSET, Theme::Effects::SHADOW_COLOR);

    // banner body
    RenderUtil::drawRoundedRect(renderer, rect, Theme::BTN_RADIUS, fill, border);

    // glow layers then crisp text on top
    if (font) {
        for (int i = Theme::Effects::BANNER_TEXT_GLOW_LAYERS; i >= 1; i--) {
            Uint8 alpha = static_cast<Uint8>(
                Theme::Effects::BANNER_TEXT_GLOW_BASE_ALPHA +
                (Theme::Effects::BANNER_TEXT_GLOW_LAYERS - i) * Theme::Effects::BANNER_TEXT_GLOW_STEP_ALPHA
            );
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