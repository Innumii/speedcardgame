#include "render/RenderButton.hpp"
#include "render/RenderText.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>

void RenderButton::drawButton(
    SDL_Renderer* renderer,
    const SDL_Rect& rect,
    const std::string& text,
    bool highlighted,
    bool pressed,
    SDL_Color baseColor,
    SDL_Color highlightColor,
    SDL_Color pressedColor,
    SDL_Color textColor,
    TTF_Font* font
) {
    if (!renderer) return;

    SDL_Color fillColor = highlighted ? highlightColor : baseColor;
    if (pressed) {
        fillColor = pressedColor;
    }

    SDL_SetRenderDrawColor(renderer, fillColor.r, fillColor.g, fillColor.b, fillColor.a);
    SDL_RenderFillRect(renderer, &rect);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &rect);

    if (font) {
        RenderText textRenderer;
        int textW = 0;
        int textH = 0;
        TTF_SizeUTF8(font, text.c_str(), &textW, &textH);
        int textX = rect.x + (rect.w - textW) / 2;
        int textY = rect.y + (rect.h - textH) / 2;
        textRenderer.drawText(renderer, text, font, textColor, textX, textY);
    }
}