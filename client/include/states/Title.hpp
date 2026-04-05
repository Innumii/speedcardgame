#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <memory>
#include <vector>
#include <string>
#include <algorithm>
#include <unordered_map>

#include "states/StateInterface.hpp"
#include "render/RenderText.hpp"
#include "objects/Card.h"
#include "render/Theme.hpp"

class Game;

class Title : public StateInterface {
public:
    void enter(Game& game) override;
    void handleEvents(Game& game, const SDL_Event& event) override;
    void update(Game& game) override;
    void render(Game& game) override;

private:
    // ─── SDL texture RAII handle ──────────────────────────────────────
    using TexPtr = std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)>;

    // ─── Showcase animation constants ─────────────────────────────────
    static constexpr float kShowcaseW        = Theme::Title::CARD_WIDTH;
    static constexpr float kShowcaseH        = kShowcaseW * Theme::PREVIEW_ASPECT_RATIO;

    static constexpr float kFlipSpeed        = 0.85f;   // ← tune flip cadence here
    static constexpr float kFlipAxisAngleDeg = 15.0f;
    static constexpr float kFloatSpeed       = 0.75f;
    static constexpr float kFloatAmpRef      = 12.0f;   // pixels at ref scale

    static constexpr float kCardSpeedRef     = 150.0f;  // ← tune travel speed here
    static constexpr float kCardGapRef       = 280.0f;  // ← tune inter-card gap here

    // ── Fade thresholds (as fraction of total travel, 0 = right, 1 = left) ──
    static constexpr float kCardFadeInPhase  = 0.10f;   // fully opaque by this phase
    static constexpr float kCardFadeOutPhase = 0.78f;   // begin fade-out at this phase

    // ─── Texture cache ────────────────────────────────────────────────
    // Keyed by card ID.  The shared card back is stored under key -1.
    // The whole cache is invalidated whenever target texture dimensions
    // change (window resize), so entries are always the right pixel size.
    struct CachedCardTex {
        TexPtr front { nullptr, SDL_DestroyTexture };
        TexPtr back  { nullptr, SDL_DestroyTexture };
    };
    std::unordered_map<int, CachedCardTex> cardTexCache_;
    int cachedTexW_ = 0;   // dimensions at which the cache was last built
    int cachedTexH_ = 0;

    // ─── Per-card conveyor state ──────────────────────────────────────
    // Textures are NOT stored here; look them up in cardTexCache_ by card ID.
    struct ActiveCard {
        int cardId = -1;
        float birthTime  = 0.0f;   // showcase.animTime when card entered from right
        float flipOffset = 0.0f;   // per-card phase offset for flip + float bob

        ActiveCard() = default;
        ActiveCard(ActiveCard&&) = default;
        ActiveCard& operator=(ActiveCard&&) = default;
        ActiveCard(const ActiveCard&) = delete;
        ActiveCard& operator=(const ActiveCard&) = delete;
    };

    // ─── Whole-showcase state ─────────────────────────────────────────
    struct ShowcaseState {
        std::vector<std::shared_ptr<Card>> deck;          // shuffled full deck
        std::unordered_map<int, std::shared_ptr<Card>> cardLookup; // ← ADD THIS

        int                                nextIdx       = 0;
        std::vector<ActiveCard>            active;
        float                              animTime      = 0.0f;
        Uint32                             lastTick      = 0;
        float                              lastSpawnTime = 0.0f;
        bool                               deckReady     = false;
        bool                               prePopDone    = false;
    } showcase;
    std::shared_ptr<Card> getCardById(int cardId) const;
    
    // ─── Layout ───────────────────────────────────────────────────────
    SDL_Point titlePos          {};
    SDL_Rect  startButton       {};
    SDL_Rect  BuildDeckButton   {};
    SDL_Rect  OpenPacksButton   {};
    SDL_Rect  ShopButton        {};
    SDL_Rect  settingsButton    {};
    SDL_Rect  logoutButton      {};
    SDL_Rect  quitButton        {};
    int       buttonGroupCenterY = 0;
    int       buttonAreaRight    = 0;
    int       hoveredButton      = -1;

    // ─── Settings modal ───────────────────────────────────────────────
    bool     settingsOpen        = false;
    bool     draggingMusicSlider = false;
    bool     draggingSFXSlider   = false;
    SDL_Rect settingsModal       {};
    SDL_Rect musicSliderTrack    {};
    SDL_Rect sfxSliderTrack      {};
    SDL_Rect settingsCloseButton {};

    // ─── Entry animation ──────────────────────────────────────────────
    bool   animInitialized = false;
    Uint32 animStartTick   = 0;

    // ─── Private helpers ──────────────────────────────────────────────
    void updateLayout(SDL_Renderer* renderer, TTF_Font* titleFont, TTF_Font* menuFont);
    void updateSettingsLayout(int screenW, int screenH);

    static int sliderVolumeFromMouseX(const SDL_Rect& track, int mouseX);

    void renderSlider(SDL_Renderer*      renderer,
                      TTF_Font*          labelFont,
                      RenderText&        textRenderer,
                      const SDL_Rect&    track,
                      int                volume,
                      const std::string& label) const;

    /// Shuffle the player's deck into showcase.deck and reset conveyor state.
    void prepareShowcaseDeck(Game& game);

    /// Return (building if necessary) the cached texture entry for cardId.
    /// Pass cardId = -1 to get/build the shared card-back entry.
    CachedCardTex& getOrBuildCardTex(SDL_Renderer* renderer, Game& game,
                                     int cardId,
                                     const std::shared_ptr<Card>& card,
                                     int texW, int texH);

    /// Advance the conveyor clock, spawn/expire cards, and draw them.
    void renderShowcase(SDL_Renderer* renderer, Game& game,
                        int screenW, int screenH, float elapsed);
};