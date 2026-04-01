#ifndef DISCARD_ANIMATION_HPP
#define DISCARD_ANIMATION_HPP

#include "AnimationInterface.hpp"
#include <SDL2/SDL.h>
#include <cstddef>
#include <memory>
#include <unordered_map>
#include "objects/Card.h"

class DiscardAnimation : public AnimationInterface {
    SDL_Rect rect{0, 0, 0, 0};
    float durationSeconds{0.0F};
    float elapsedSeconds{0.0F};
    bool finished{false};
    int cardId{-1};
    std::unique_ptr<Card> ownedCard{nullptr};  // add this

    mutable SDL_Texture* cachedTexture{nullptr};
    bool textureDirty{true};    

    static std::unordered_map<int, std::pair<std::shared_ptr<DiscardAnimation>, Uint32>> sPending;

public:
    DiscardAnimation(const SDL_Rect& cardRect, int cardId, Uint32 durationMs);
    ~DiscardAnimation();

    void drawDisintegration(SDL_Renderer* renderer, Uint32 now) const;


    void start() override;
    void update(float dt) override;
    bool isFinished() const override;
    bool isBlocking() const override { return false; }

    SDL_Rect getRect() const;
    float getProgress() const;
    int getCardId() const;
    const Card* getOwnedCard() const;  // add this

    static bool hasPending(int cardId);
    static void stagePending(const SDL_Rect& rect, int cardId, Uint32 durationMs, std::unique_ptr<Card> card = nullptr);
    static std::shared_ptr<DiscardAnimation> takePending(int cardId);
    static void cleanupStalePending(Uint32 now);
    
    SDL_Texture* getCachedTexture() const { return cachedTexture; }
    void setCachedTexture(SDL_Texture* t) const { cachedTexture = t; }
};

#endif