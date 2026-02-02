#ifndef RENDERTEXT_HPP
#define RENDERTEXT_HPP
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <cstddef>
#include <string>

// Stateless helpers for rendering text
class RenderText {
public:
    static void drawText(SDL_Renderer* renderer, const std::string& text, TTF_Font* font, SDL_Color color, int x, int y);
    static void drawWrappedText(SDL_Renderer* renderer, const std::string& text, TTF_Font* font, SDL_Color color, int x, int y, std::size_t maxLineLen);
};

#endif