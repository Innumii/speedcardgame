#pragma once

#include "StateInterface.hpp"
#include "render/RenderText.hpp"
#include <SDL2/SDL.h>
#include <array>
#include <string>

class Game;

class Title : public StateInterface {
public:
    void enter(Game& game) override;
    void handleEvents(Game& game, const SDL_Event& event) override;
    void update(Game& game) override;
    void render(Game& game) override;
    ~Title() override;

private:
    // ── Layout ───────────────────────────────────────────────────────
    void updateLayout(SDL_Renderer* renderer);

    SDL_Rect startButton{};
    SDL_Rect BuildDeckButton{};
    SDL_Rect OpenPacksButton{};
    SDL_Rect ShopButton{};
    SDL_Rect logoutButton{};
    SDL_Rect settingsButton{};   // NEW — sits between logout and quit
    SDL_Rect quitButton{};
    SDL_Rect titleBanner{};

    // ── Button texture cache ─────────────────────────────────────────
    // Indices: 0 Start | 1 Build | 2 Packs | 3 Shop | 4 Logout | 5 Settings | 6 Quit
    struct ButtonCache {
        SDL_Texture* texture = nullptr;
        int          w       = 0;
        int          h       = 0;
        std::string  label;
        bool         hovered  = false;
        float        lastScale = 0.0f;
    };
    std::array<ButtonCache, 7> cachedButtons{};

    int hoveredButton = -1;

    SDL_Texture* buildButtonTexture(SDL_Renderer* renderer,
                                    const SDL_Rect& rect,
                                    const std::string& text,
                                    SDL_Color fill,
                                    bool hovered,
                                    const RenderText::FontSet& fonts);

    // ── Animation ────────────────────────────────────────────────────
    bool   animInitialized = false;
    Uint32 animStartTick   = 0;

    // ── Settings modal ───────────────────────────────────────────────
    bool     settingsOpen        = false;
    SDL_Rect settingsModal{};
    SDL_Rect settingsCloseButton{};

    // Slider tracks (the full bar the thumb slides along)
    SDL_Rect musicSliderTrack{};
    SDL_Rect sfxSliderTrack{};

    // Active drag state — only one slider can be dragged at a time
    bool draggingMusicSlider = false;
    bool draggingSFXSlider   = false;

    // Helper: recompute modal / slider rects from current screen size
    void updateSettingsLayout(int screenW, int screenH);

    // Helper: clamp mouse X into [track.x, track.x+track.w] → volume 0-128
    static int sliderVolumeFromMouseX(const SDL_Rect& track, int mouseX);

    // Helper: render a single labelled volume slider
    void renderSlider(SDL_Renderer* renderer,
                      TTF_Font*     labelFont,
                      RenderText&   textRenderer,
                      const SDL_Rect& track,
                      int            volume,
                      const std::string& label) const;
};