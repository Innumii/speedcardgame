#ifndef DRAW_CARD_ANIMATION_HPP
#define DRAW_CARD_ANIMATION_HPP

#include <SDL2/SDL.h>

class DrawCardAnimation {
public:
    void start(const SDL_Rect& from, const SDL_Rect& to, Uint32 now, Uint32 duration);
    void update(Uint32 now);
    bool isActive() const;
    const SDL_Rect& getCurrentRect() const;

private:
    SDL_Rect fromRect{0, 0, 0, 0};
    SDL_Rect toRect{0, 0, 0, 0};
    SDL_Rect currentRect{0, 0, 0, 0};
    Uint32 startTick{0};
    Uint32 durationMs{1};
    bool active{false};
};

#endif
