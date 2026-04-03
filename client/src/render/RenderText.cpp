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

    bool measureTextImpl(TTF_Font* font, const std::string& text, int& w, int& h) {
        if (!font) return false;
        if (TTF_SizeUTF8(font, text.c_str(), &w, &h) != 0) { w = h = 0; return false; }
        return true;
    }

    std::string truncateWithEllipsisImpl(TTF_Font* font, const std::string& text, int maxWidth) {
        if (!font || maxWidth <= 0) return {};
        int w = 0, h = 0;
        if (measureTextImpl(font, text, w, h) && w <= maxWidth) return text;
        const std::string ellipsis = "...";
        int ew = 0, eh = 0;
        if (!measureTextImpl(font, ellipsis, ew, eh) || ew > maxWidth) return {};
        std::string t = text;
        while (!t.empty()) {
            t.pop_back();
            std::string c = t + ellipsis;
            if (measureTextImpl(font, c, w, h) && w <= maxWidth) return c;
        }
        return ellipsis;
    }
}

bool RenderText::measureText(TTF_Font* font, const std::string& text, int& w, int& h) {
    return measureTextImpl(font, text, w, h);
}

std::string RenderText::truncateWithEllipsis(TTF_Font* font, const std::string& text, int maxWidth) {
    return truncateWithEllipsisImpl(font, text, maxWidth);
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

void RenderText::drawWrappedText(SDL_Renderer* renderer, const std::string& text, 
                                 TTF_Font* font, SDL_Color color, int x, int y, int maxWidth) {
    if (!renderer || !font) return;

    std::istringstream words(text);
    std::string word;
    std::string line;
    int lineSkip = TTF_FontLineSkip(font);
    int yOffset = y;

    while (words >> word) {
        std::string testLine = line.empty() ? word : line + " " + word;

        int w = 0, h = 0;
        TTF_SizeUTF8(font, testLine.c_str(), &w, &h);
        if (w > maxWidth && !line.empty()) {
            // draw the current line
            drawText(renderer, line, font, color, x, yOffset);
            yOffset += lineSkip;
            line = word; // start new line
        } else {
            line = testLine;
        }
    }

    if (!line.empty()) {
        drawText(renderer, line, font, color, x, yOffset);
    }
}