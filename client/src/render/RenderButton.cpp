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
    Style style{};
    style.fill = fill;
    style.border = border;
    style.text = textColor;
    drawButton(renderer, rect, text, font, style, hovered, pressed);
}

void RenderButton::drawButton(
    SDL_Renderer* renderer,
    const SDL_Rect& rect,
    const std::string& text,
    TTF_Font* font,
    const Style& style,
    bool hovered,
    bool pressed
) {
    if (!renderer) return;

    const int rad = style.radius >= 0 ? style.radius : Theme::BTN_RADIUS;

    SDL_Color finalFill = style.fill;
    SDL_Color finalBorder = style.border;
    SDL_Color finalText = style.text;

    if (pressed) {
        if (style.hasPressedOverride) {
            finalFill = style.pressedFill;
            finalBorder = style.pressedBorder;
            finalText = style.pressedText;
        } else {
            finalFill = RenderUtil::darken(style.fill, 30);
        }
    } else if (hovered) {
        if (style.hasHoverOverride) {
            finalFill = style.hoverFill;
            finalBorder = style.hoverBorder;
            finalText = style.hoverText;
        } else {
            finalFill = RenderUtil::brighten(style.fill, 45);
        }
    }

    if (hovered && style.drawGlowOnHover) {
        RenderUtil::drawRoundedGlow(
            renderer,
            rect,
            rad,
            finalFill,
            Theme::Effects::BUTTON_GLOW_LAYERS,
            Theme::Effects::BUTTON_GLOW_MAX_ALPHA
        );
    }

    if (style.drawShadow) {
        RenderUtil::drawRoundedShadow(renderer, rect, rad, Theme::Effects::SHADOW_OFFSET, Theme::Effects::SHADOW_COLOR);
    }

    RenderUtil::drawRoundedRect(renderer, rect, rad, finalFill, finalBorder);

    if (pressed && !style.hasPressedOverride) {
        SDL_Color inset = RenderUtil::darken(finalBorder, 40);
        SDL_SetRenderDrawColor(renderer, inset.r, inset.g, inset.b, inset.a);
        SDL_RenderDrawLine(renderer, rect.x + rad, rect.y + 1,
                           rect.x + rect.w - rad, rect.y + 1);
    }

    RenderUtil::drawCenteredText(renderer, font, text, rect, finalText);
}