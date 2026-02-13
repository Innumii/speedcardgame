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
        bool highlighted,
        bool pressed,
        SDL_Color baseColor,
        SDL_Color highlightColor,
        SDL_Color pressedColor,
        SDL_Color textColor,
        TTF_Font* font
    );
};

#endif