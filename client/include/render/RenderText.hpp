#ifndef RENDERTEXT_HPP
#define RENDERTEXT_HPP
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <cstddef>
#include <string>

// Stateless helpers for rendering text
class RenderText {
public:
    struct FontSet {
        TTF_Font* small = nullptr;
        TTF_Font* tiny = nullptr;
        TTF_Font* medium = nullptr; 
        TTF_Font* large = nullptr;
    };

    static bool ensureTtfReady();
    static void shutdownTtf();
    static FontSet loadFonts(const char* path, int smallSize, int tinySize, int largeSize);
    static void closeFonts(FontSet& fonts);
    static void drawText(SDL_Renderer* renderer, const std::string& text, TTF_Font* font, SDL_Color color, int x, int y);
    static void drawWrappedText(SDL_Renderer* renderer, const std::string& text, 
                                 TTF_Font* font, SDL_Color color, int x, int y, int maxWidth);
    static bool measureText(TTF_Font* font, const std::string& text, int& w, int& h);
    static std::string truncateWithEllipsis(TTF_Font* font, const std::string& text, int maxWidth);
};

#endif