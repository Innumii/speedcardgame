#ifndef RENDERBANNER_HPP
#define RENDERBANNER_HPP

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>

class RenderBanner {
public:
    static void drawBanner(
        SDL_Renderer* renderer,
        const SDL_Rect& rect,
        const std::string& text,
        TTF_Font* font,
        SDL_Color fill,
        SDL_Color border,
        SDL_Color textColor,
        SDL_Color glowColor
    );
};

#endif