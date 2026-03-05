#ifndef RENDERUTIL_HPP
#define RENDERUTIL_HPP

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>

namespace RenderUtil {
    void fillCircle(SDL_Renderer* renderer, int centerX, int centerY, int radius);
    void fillRoundedRect(SDL_Renderer* renderer, const SDL_Rect& rect, int radius, SDL_Color fill);
    void drawRoundedBorder(SDL_Renderer* renderer, const SDL_Rect& rect, int radius, SDL_Color border, int thickness);
    void drawRoundedRect(SDL_Renderer* renderer, const SDL_Rect& rect, int radius, SDL_Color fill, SDL_Color border);
    void drawCenteredText(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, const SDL_Rect& rect, SDL_Color color);

    SDL_Color brighten(SDL_Color color, int amount);
    SDL_Color darken(SDL_Color color, int amount);
}

#endif