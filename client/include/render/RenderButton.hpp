#ifndef RENDERBUTTON_HPP
#define RENDERBUTTON_HPP

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>

class RenderButton {
public:
    struct Style {
        SDL_Color fill;
        SDL_Color border;
        SDL_Color text;

        SDL_Color hoverFill;
        SDL_Color hoverBorder;
        SDL_Color hoverText;
        bool hasHoverOverride = false;

        SDL_Color pressedFill;
        SDL_Color pressedBorder;
        SDL_Color pressedText;
        bool hasPressedOverride = false;

        int radius = -1;
        bool drawShadow = true;
        bool drawGlowOnHover = true;
    };

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

    static void drawButton(
        SDL_Renderer* renderer,
        const SDL_Rect& rect,
        const std::string& text,
        TTF_Font* font,
        const Style& style,
        bool hovered = false,
        bool pressed = false
    );
};

#endif