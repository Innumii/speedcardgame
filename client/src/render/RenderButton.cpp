#include "render/RenderButton.hpp"
#include "render/Theme.hpp"
#include "utils/RenderUtil.hpp"

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
        RenderUtil::drawRoundedGlow(
            renderer,
            rect,
            rad,
            finalFill,
            Theme::Effects::BUTTON_GLOW_LAYERS,
            Theme::Effects::BUTTON_GLOW_MAX_ALPHA
        );
    }

    // shadow
    RenderUtil::drawRoundedShadow(renderer, rect, rad, Theme::Effects::SHADOW_OFFSET, Theme::Effects::SHADOW_COLOR);

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