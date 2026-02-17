#include "render/RenderText.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
    std::vector<std::string> wrapWords(const std::string& text, std::size_t maxLineLen) {
        std::vector<std::string> lines;
        std::istringstream words(text);
        std::string word;
        std::string current;

        while (words >> word) {
            const bool fitsOnLine = current.size() + (current.empty() ? 0 : 1) + word.size() <= maxLineLen;
            if (!current.empty() && !fitsOnLine) {
                lines.push_back(current);
                current.clear();
            }

            if (!current.empty()) current.push_back(' ');
            current.append(word);
        }

        if (!current.empty()) {
            lines.push_back(current);
        }

        return lines;
    }
}

bool RenderText::ensureTtfReady() {
    return TTF_WasInit() || TTF_Init() == 0;
}

void RenderText::shutdownTtf() {
    if (TTF_WasInit()) {
        TTF_Quit();
    }
}

RenderText::FontSet RenderText::loadFonts(const char* path, int smallSize, int tinySize, int largeSize) {
    FontSet fonts;
    if (!path || !ensureTtfReady()) return fonts;

    fonts.small = TTF_OpenFont(path, smallSize);
    fonts.tiny = TTF_OpenFont(path, tinySize);
    fonts.medium = TTF_OpenFont(path, (smallSize + largeSize) / 2);
    fonts.large = TTF_OpenFont(path, largeSize);

    return fonts;
}

void RenderText::closeFonts(FontSet& fonts) {
    if (fonts.small) {
        TTF_CloseFont(fonts.small);
        fonts.small = nullptr;
    }
    if (fonts.tiny) {
        TTF_CloseFont(fonts.tiny);
        fonts.tiny = nullptr;
    }
    if (fonts.medium) {
        TTF_CloseFont(fonts.medium);
        fonts.medium = nullptr;
    }
    if (fonts.large) {
        TTF_CloseFont(fonts.large);
        fonts.large = nullptr;
    }
}

void RenderText::drawText(SDL_Renderer* renderer, const std::string& text, TTF_Font* font, SDL_Color color, int x, int y) {
    if (!renderer || !font || text.empty()) return;

    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!surface) {
        std::cerr << "TTF_RenderUTF8_Blended failed: " << TTF_GetError() << '\n';
        return;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        std::cerr << "SDL_CreateTextureFromSurface failed: " << SDL_GetError() << '\n';
        SDL_FreeSurface(surface);
        return;
    }

    SDL_Rect dst{x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, nullptr, &dst);

    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

void RenderText::drawWrappedText(SDL_Renderer* renderer, const std::string& text, TTF_Font* font, SDL_Color color, int x, int y, std::size_t maxLineLen) {
    if (!renderer || !font) return;

    const auto lines = wrapWords(text, maxLineLen);
    int lineSkip = TTF_FontLineSkip(font);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        drawText(renderer, lines[i], font, color, x, y + static_cast<int>(i) * lineSkip);
    }
}