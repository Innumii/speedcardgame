#ifndef RENDERBUTTON_HPP
#define RENDERBUTTON_HPP

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>

class RenderButton {
public:
    static void drawButton(
        SDL_Renderer* renderer,
        const SDL_Rect& rect,
        const std::string& text,
        TTF_Font* font,
        SDL_Color fill,
        SDL_Color border,
        SDL_Color textColor,
        bool hovered = false,
        bool pressed = false
    );
};

#endif