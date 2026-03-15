#ifndef RENDERTARGETING_HPP
#define RENDERTARGETING_HPP

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

class Playing;
class RenderText;

// Responsible for rendering targeting overlays and target prompts in Playing.
class RenderTargeting {
public:
	static void drawPendingTargeting(SDL_Renderer* renderer, RenderText& textRenderer, const Playing& playing, TTF_Font* fontSmall);
};

#endif
