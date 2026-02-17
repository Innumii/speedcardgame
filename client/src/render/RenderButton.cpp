#include "render/RenderButton.hpp"
#include "render/Theme.hpp"
#include <cmath>
#include <algorithm>

// ── file-local primitives ─────────────────────────────────────────────────────

namespace {
    void fillCircle(SDL_Renderer* r, int cx, int cy, int rad) {
        for (int dy = -rad; dy <= rad; dy++) {
            int dx = (int)sqrt((double)(rad*rad - dy*dy));
            SDL_RenderDrawLine(r, cx-dx, cy+dy, cx+dx, cy+dy);
        }
    }

    void drawRoundedRect(SDL_Renderer* r, const SDL_Rect& rect,
                          int rad, SDL_Color fill, SDL_Color border) {
        SDL_SetRenderDrawColor(r, fill.r, fill.g, fill.b, fill.a);
        SDL_Rect body  = {rect.x + rad,          rect.y,       rect.w - 2*rad, rect.h        };
        SDL_Rect left  = {rect.x,                 rect.y + rad, rad,            rect.h - 2*rad};
        SDL_Rect right = {rect.x + rect.w - rad,  rect.y + rad, rad,            rect.h - 2*rad};
        SDL_RenderFillRect(r, &body);
        SDL_RenderFillRect(r, &left);
        SDL_RenderFillRect(r, &right);
        fillCircle(r, rect.x + rad,          rect.y + rad,          rad);
        fillCircle(r, rect.x + rect.w - rad, rect.y + rad,          rad);
        fillCircle(r, rect.x + rad,          rect.y + rect.h - rad, rad);
        fillCircle(r, rect.x + rect.w - rad, rect.y + rect.h - rad, rad);

        SDL_SetRenderDrawColor(r, border.r, border.g, border.b, border.a);
        SDL_RenderDrawLine(r, rect.x + rad,      rect.y,          rect.x + rect.w - rad, rect.y            );
        SDL_RenderDrawLine(r, rect.x + rad,      rect.y + rect.h, rect.x + rect.w - rad, rect.y + rect.h   );
        SDL_RenderDrawLine(r, rect.x,            rect.y + rad,    rect.x,                rect.y + rect.h - rad);
        SDL_RenderDrawLine(r, rect.x + rect.w,   rect.y + rad,    rect.x + rect.w,       rect.y + rect.h - rad);
    }

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
        fillCircle(r, s.x + rad,       s.y + rad,       rad);
        fillCircle(r, s.x + s.w - rad, s.y + rad,       rad);
        fillCircle(r, s.x + rad,       s.y + s.h - rad, rad);
        fillCircle(r, s.x + s.w - rad, s.y + s.h - rad, rad);
    }

    void drawGlowHalo(SDL_Renderer* r, const SDL_Rect& rect,
                       int rad, SDL_Color glowColor, float pulse) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        for (int i = 6; i >= 1; i--) {
            Uint8 alpha = (Uint8)(pulse * 60.0f * (6 - i) / 6.0f);
            SDL_Color halo = {glowColor.r, glowColor.g, glowColor.b, alpha};
            SDL_Rect haloRect = {rect.x - i, rect.y - i,
                                 rect.w + i*2, rect.h + i*2};
            drawRoundedRect(r, haloRect, rad, {0,0,0,0}, halo);
        }
    }

    void drawCentered(SDL_Renderer* r, TTF_Font* font,
                       const std::string& text, const SDL_Rect& rect, SDL_Color color) {
        if (!font) return;
        SDL_Surface* s = TTF_RenderUTF8_Blended(font, text.c_str(), color);
        if (!s) return;
        SDL_Texture* t = SDL_CreateTextureFromSurface(r, s);
        if (t) {
            SDL_Rect dst = {rect.x + (rect.w - s->w)/2,
                            rect.y + (rect.h - s->h)/2,
                            s->w, s->h};
            SDL_RenderCopy(r, t, nullptr, &dst);
            SDL_DestroyTexture(t);
        }
        SDL_FreeSurface(s);
    }

    SDL_Color brighten(SDL_Color c, int amt) {
        return {(Uint8)std::min(c.r + amt, 255),
                (Uint8)std::min(c.g + amt, 255),
                (Uint8)std::min(c.b + amt, 255), c.a};
    }

    SDL_Color darken(SDL_Color c, int amt) {
        return {(Uint8)std::max(c.r - amt, 0),
                (Uint8)std::max(c.g - amt, 0),
                (Uint8)std::max(c.b - amt, 0), c.a};
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
    SDL_Color finalFill = pressed  ? darken(fill, 30)
                        : hovered  ? brighten(fill, 45)
                        : fill;

    // hover glow halo
    if (hovered) {
        const float pulse = 0.7f;  // static pulse — animate externally if needed
        drawGlowHalo(renderer, rect, rad, finalFill, pulse);
    }

    // shadow
    drawShadow(renderer, rect, rad);

    // button body
    drawRoundedRect(renderer, rect, rad, finalFill, border);

    // pressed — slight inset effect via darker top border
    if (pressed) {
        SDL_Color inset = darken(border, 40);
        SDL_SetRenderDrawColor(renderer, inset.r, inset.g, inset.b, inset.a);
        SDL_RenderDrawLine(renderer, rect.x + rad, rect.y + 1,
                           rect.x + rect.w - rad, rect.y + 1);
    }

    // centered text
    drawCentered(renderer, font, text, rect, textColor);
}