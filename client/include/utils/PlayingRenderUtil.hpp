#ifndef PLAYING_RENDER_UTIL_HPP
#define PLAYING_RENDER_UTIL_HPP

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <cstddef>
#include <memory>
#include <vector>

class AnimationInterface;
class DrawCardAnimation;
class RenderText;

namespace PlayingRenderUtil {
    struct AttackRenderFrame {
        SDL_Rect rect{0, 0, 0, 0};
        Uint8 alpha{0};
        int lane{-1};
        bool selfPlayer{false};
    };

    struct DeathRenderFrame {
        SDL_Rect rect{0, 0, 0, 0};
        Uint8 alpha{0};
    };

    void collectAttackFrames(const std::shared_ptr<const AnimationInterface>& animation,
                             std::vector<AttackRenderFrame>& frames);

    void collectDeathFrames(const std::shared_ptr<const AnimationInterface>& animation,
                            std::vector<DeathRenderFrame>& frames);

    std::shared_ptr<const DrawCardAnimation> findDrawAnimation(
        const std::shared_ptr<const AnimationInterface>& animation);

    void drawOpponentDeck(SDL_Renderer* renderer, RenderText& textRenderer,
                        const std::vector<SDL_Rect>& opponentSlots,
                        std::size_t deckSize, int screenW, int screenH,
                        TTF_Font* fontSmall);

    void drawSelfDeck(SDL_Renderer* renderer, RenderText& textRenderer,
                      const std::vector<SDL_Rect>& playSlots,
                      int deckSize, int screenW, int screenH, TTF_Font* fontSmall);

    SDL_Rect computeOpponentDiscardRect(const std::vector<SDL_Rect>& opponentSlots, int screenW, int screenH);
}

#endif