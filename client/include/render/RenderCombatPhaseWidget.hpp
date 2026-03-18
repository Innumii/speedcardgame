#ifndef RENDER_COMBAT_PHASE_WIDGET_HPP
#define RENDER_COMBAT_PHASE_WIDGET_HPP

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>

class RenderText;

class RenderCombatPhaseWidget {
public:
    static void draw(SDL_Renderer* renderer, RenderText& textRenderer, TTF_Font* font,
                     int screenW, int screenH,
                     const std::vector<SDL_Rect>& opponentSlots,
                     const std::vector<SDL_Rect>& playSlots,
                     bool combatPhaseActive, float barProgress,
                     const std::string& combatLabel);
};

#endif