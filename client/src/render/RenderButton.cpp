#include "render/RenderButton.hpp"
#include "render/Theme.hpp"
#include "utils/RenderUtil.hpp"

// ── file-local primitives ─────────────────────────────────────────────────────

namespace {
    void drawShadow(SDL_Renderer* r, const SDL_Rect& rect, int rad) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 90);
        SDL_Rect s  = {rect.x + 5, rect.y + 5, rect.w, rect.h};
        SDL_Rect sb = {s.x + rad,        s.y,     s.w - 2*rad, s.h        };
        SDL_Rect sl = {s.x,              s.y+rad, rad,         s.h - 2*rad};
        SDL_Rect sr = {s.x + s.w - rad,  s.y+rad, rad,         s.h - 2*rad};
        SDL_RenderFillRect(r, &sb);
        SDL_RenderFillRect(r, &sl);
        SDL_RenderFillRect(r, &sr);
        RenderUtil::fillCircle(r, s.x + rad,       s.y + rad,       rad);
        RenderUtil::fillCircle(r, s.x + s.w - rad, s.y + rad,       rad);
        RenderUtil::fillCircle(r, s.x + rad,       s.y + s.h - rad, rad);
        RenderUtil::fillCircle(r, s.x + s.w - rad, s.y + s.h - rad, rad);
    }

    void drawGlowHalo(SDL_Renderer* r, const SDL_Rect& rect,
                       int rad, SDL_Color glowColor, float pulse) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        for (int i = 6; i >= 1; i--) {
            Uint8 alpha = (Uint8)(pulse * 60.0f * (6 - i) / 6.0f);
            SDL_Color halo = {glowColor.r, glowColor.g, glowColor.b, alpha};
            SDL_Rect haloRect = {rect.x - i, rect.y - i,
                                 rect.w + i*2, rect.h + i*2};
            RenderUtil::drawRoundedRect(r, haloRect, rad, {0,0,0,0}, halo);
        }
    }

}

// ── RenderButton::drawButton ──────────────────────────────────────────────────

void RenderButton::drawButton(
    SDL_Renderer* renderer,
    const SDL_Rect& rect,
    const std::string& text,
    TTF_Font* font,
    SDL_Color fill,
    SDL_Color border,
    SDL_Color textColor,
    bool hovered,
    bool pressed
) {
    if (!renderer) return;

    const int rad = Theme::BTN_RADIUS;

    // resolve final fill based on state
    SDL_Color finalFill = pressed  ? RenderUtil::darken(fill, 30)
                        : hovered  ? RenderUtil::brighten(fill, 45)
                        : fill;

    // hover glow halo
    if (hovered) {
        const float pulse = 0.7f;  // static pulse — animate externally if needed
        drawGlowHalo(renderer, rect, rad, finalFill, pulse);
    }

    // shadow
    drawShadow(renderer, rect, rad);

    // button body
    RenderUtil::drawRoundedRect(renderer, rect, rad, finalFill, border);

    // pressed — slight inset effect via darker top border
    if (pressed) {
        SDL_Color inset = RenderUtil::darken(border, 40);
        SDL_SetRenderDrawColor(renderer, inset.r, inset.g, inset.b, inset.a);
        SDL_RenderDrawLine(renderer, rect.x + rad, rect.y + 1,
                           rect.x + rect.w - rad, rect.y + 1);
    }

    // centered text
    RenderUtil::drawCenteredText(renderer, font, text, rect, textColor);
}