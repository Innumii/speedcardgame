#include "states/Title.hpp"
#include "core/Game.hpp"
#include "featureFlag/AnimationFlag.hpp"
#include "render/RenderBackdrop.hpp"
#include "render/RenderText.hpp"
#include "render/RenderBanner.hpp"
#include "render/RenderButton.hpp"
#include "render/Theme.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <cmath>
#include <utils/RenderUtil.hpp>
#include <core/Audio.hpp>

namespace {
    float scale = 1.0f;
}

// ─────────────────────────────────────────────────────────────────────────────
// enter
// ─────────────────────────────────────────────────────────────────────────────
void Title::enter(Game& game) {
    Audio::playMusic("title");

    // Invalidate cached button textures — SDL render-target textures can
    // become stale after window resize events that occur in other states.
    for (auto& cache : cachedButtons) {
        if (cache.texture) {
            SDL_DestroyTexture(cache.texture);
            cache.texture = nullptr;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// buildButtonTexture
// ─────────────────────────────────────────────────────────────────────────────
SDL_Texture* Title::buildButtonTexture(SDL_Renderer* renderer,
                                       const SDL_Rect& rect,
                                       const std::string& text,
                                       SDL_Color fill,
                                       bool hovered,
                                       const RenderText::FontSet& fonts)
{
    SDL_Texture* tex = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        rect.w,
        rect.h
    );
    if (!tex) return nullptr;

    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(renderer, tex);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    SDL_Rect local{0, 0, rect.w, rect.h};
    RenderButton::drawButton(
        renderer, local, text, fonts.large,
        fill, Theme::BTN_BORDER, Theme::BTN_TEXT, hovered
    );

    SDL_SetRenderTarget(renderer, nullptr);
    return tex;
}

// ─────────────────────────────────────────────────────────────────────────────
// Layout constants
// ─────────────────────────────────────────────────────────────────────────────
static constexpr float kRefW = 1200.0F;
static constexpr float kRefH = 850.0F;

// ─────────────────────────────────────────────────────────────────────────────
// updateLayout  —  all main-menu button / banner rects
// ─────────────────────────────────────────────────────────────────────────────
void Title::updateLayout(SDL_Renderer* renderer) {
    int screenW = 800;
    int screenH = 600;
    if (renderer) {
        SDL_GetRendererOutputSize(renderer, &screenW, &screenH);
    }

    scale = std::min(
        static_cast<float>(screenW) / kRefW,
        static_cast<float>(screenH) / kRefH);

    const int mainBtnW   = static_cast<int>(Theme::Title::MAIN_BUTTON_WIDTH   * scale);
    const int mainBtnH   = static_cast<int>(Theme::Title::MAIN_BUTTON_HEIGHT  * scale);
    const int smallBtnW  = static_cast<int>(Theme::Title::SMALL_BUTTON_WIDTH  * scale);
    const int smallBtnH  = static_cast<int>(Theme::Title::SMALL_BUTTON_HEIGHT * scale);
    const int bannerW    = static_cast<int>(Theme::Title::BANNER_WIDTH         * scale);
    const int bannerH    = static_cast<int>(Theme::Title::BANNER_HEIGHT        * scale);
    const int bannerGap  = static_cast<int>(24 * scale);
    const int buttonGap  = static_cast<int>(16 * scale);
    const int smallRowGap = static_cast<int>(12 * scale);

    startButton.w     = mainBtnW;   startButton.h     = mainBtnH;
    BuildDeckButton.w = mainBtnW;   BuildDeckButton.h = mainBtnH;
    OpenPacksButton.w = mainBtnW;   OpenPacksButton.h = mainBtnH;
    ShopButton.w      = mainBtnW;   ShopButton.h      = mainBtnH;
    logoutButton.w    = smallBtnW;  logoutButton.h    = smallBtnH;
    settingsButton.w  = smallBtnW;  settingsButton.h  = smallBtnH;
    quitButton.w      = smallBtnW;  quitButton.h      = smallBtnH;
    titleBanner.w     = bannerW;    titleBanner.h     = bannerH;

    const int buttonsTotalH = (mainBtnH * 4) + (buttonGap * 3) + smallBtnH;
    const int totalH        = bannerH + bannerGap + buttonsTotalH;
    int topY = (screenH - totalH) / 2;
    if (topY < static_cast<int>(20 * scale)) topY = static_cast<int>(20 * scale);

    const int centerX = screenW / 2;

    titleBanner.x     = centerX - (bannerW / 2);
    titleBanner.y     = topY;

    startButton.x     = centerX - (mainBtnW / 2);
    startButton.y     = titleBanner.y + bannerH + bannerGap;

    BuildDeckButton.x = startButton.x;
    BuildDeckButton.y = startButton.y + mainBtnH + buttonGap;

    OpenPacksButton.x = startButton.x;
    OpenPacksButton.y = BuildDeckButton.y + mainBtnH + buttonGap;

    ShopButton.x      = startButton.x;
    ShopButton.y      = OpenPacksButton.y + mainBtnH + buttonGap;

    // Bottom row: three small buttons side-by-side — Logout | Settings | Quit
    const int smallTotalW = (smallBtnW * 3) + (smallRowGap * 2);
    const int smallStartX = centerX - (smallTotalW / 2);
    const int smallY      = ShopButton.y + mainBtnH + buttonGap;

    logoutButton.x   = smallStartX;
    logoutButton.y   = smallY;

    settingsButton.x = smallStartX + smallBtnW + smallRowGap;
    settingsButton.y = smallY;

    quitButton.x     = settingsButton.x + smallBtnW + smallRowGap;
    quitButton.y     = smallY;

    // Keep the settings modal geometry in sync with the window
    updateSettingsLayout(screenW, screenH);
}

// ─────────────────────────────────────────────────────────────────────────────
// updateSettingsLayout  —  modal panel, slider tracks, close button
// ─────────────────────────────────────────────────────────────────────────────
void Title::updateSettingsLayout(int screenW, int screenH) {
    // Modal panel dimensions (reference: 480 × 280)
    const int modalW = static_cast<int>(480 * scale);
    const int modalH = static_cast<int>(280 * scale);
    settingsModal = {(screenW - modalW) / 2, (screenH - modalH) / 2, modalW, modalH};

    // Close button — top-right corner of the modal
    const int closeSz = static_cast<int>(36 * scale);
    const int closePad = static_cast<int>(10 * scale);
    settingsCloseButton = {
        settingsModal.x + settingsModal.w - closeSz - closePad,
        settingsModal.y + closePad,
        closeSz,
        closeSz
    };

    // Slider shared geometry
    const int trackH     = static_cast<int>(14 * scale);
    const int trackPadX  = static_cast<int>(40 * scale);   // left/right inset inside modal
    const int trackW     = modalW - (trackPadX * 2);
    const int labelRowH  = static_cast<int>(28 * scale);   // space above each slider for the label

    // First slider (music) — starts ~90px below the modal top
    const int firstTrackY = settingsModal.y + static_cast<int>(90 * scale);
    musicSliderTrack = {
        settingsModal.x + trackPadX,
        firstTrackY,
        trackW,
        trackH
    };

    // Second slider (SFX) — 80px below the first
    const int secondTrackY = firstTrackY + labelRowH + trackH + static_cast<int>(40 * scale);
    sfxSliderTrack = {
        settingsModal.x + trackPadX,
        secondTrackY,
        trackW,
        trackH
    };
}

// ─────────────────────────────────────────────────────────────────────────────
// sliderVolumeFromMouseX
// ─────────────────────────────────────────────────────────────────────────────
int Title::sliderVolumeFromMouseX(const SDL_Rect& track, int mouseX) {
    const float ratio = static_cast<float>(mouseX - track.x) /
                        static_cast<float>(track.w);
    return static_cast<int>(std::clamp(ratio, 0.0f, 1.0f) * 128.0f);
}

// ─────────────────────────────────────────────────────────────────────────────
// handleEvents
// ─────────────────────────────────────────────────────────────────────────────
void Title::handleEvents(Game& game, const SDL_Event& event) {
    updateLayout(game.getRenderer());

    if (event.type == SDL_QUIT) {
        game.setNextState(GameState::Quit);
        return;
    }

    // ── Settings modal is open — intercept all mouse events ──────────
    if (settingsOpen) {
        switch (event.type) {
            case SDL_MOUSEBUTTONDOWN: {
                if (event.button.button != SDL_BUTTON_LEFT) break;
                const int mx = event.button.x;
                const int my = event.button.y;

                // Close button
                if (RenderUtil::pointInRect(settingsCloseButton, mx, my)) {
                    settingsOpen = false;
                    draggingMusicSlider = false;
                    draggingSFXSlider   = false;
                    break;
                }

                // Click anywhere on the music track — jump thumb + begin drag
                if (RenderUtil::pointInRect(musicSliderTrack, mx, my)) {
                    Audio::setMusicVolume(sliderVolumeFromMouseX(musicSliderTrack, mx));
                    draggingMusicSlider = true;
                    break;
                }

                // Click anywhere on the SFX track
                if (RenderUtil::pointInRect(sfxSliderTrack, mx, my)) {
                    Audio::setSFXVolume(sliderVolumeFromMouseX(sfxSliderTrack, mx));
                    draggingSFXSlider = true;
                    break;
                }

                // Click outside modal closes it
                if (!RenderUtil::pointInRect(settingsModal, mx, my)) {
                    settingsOpen = false;
                }
                break;
            }

            case SDL_MOUSEMOTION: {
                if (draggingMusicSlider) {
                    Audio::setMusicVolume(sliderVolumeFromMouseX(musicSliderTrack, event.motion.x));
                } else if (draggingSFXSlider) {
                    Audio::setSFXVolume(sliderVolumeFromMouseX(sfxSliderTrack, event.motion.x));
                }
                break;
            }

            case SDL_MOUSEBUTTONUP: {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    draggingMusicSlider = false;
                    draggingSFXSlider   = false;
                }
                break;
            }

            default: break;
        }
        return; // swallow all events while settings is open
    }

    // ── Normal title-screen click handling ───────────────────────────
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        const int mouseX = event.button.x;
        const int mouseY = event.button.y;

        auto inRect = [&](const SDL_Rect& r) {
            return mouseX >= r.x && mouseX <= r.x + r.w &&
                   mouseY >= r.y && mouseY <= r.y + r.h;
        };

        if (inRect(startButton)) {
            game.setNextState(GameState::Connecting);
        } else if (inRect(BuildDeckButton)) {
            game.setNextState(GameState::DeckBuilding);
        } else if (inRect(OpenPacksButton)) {
            game.setNextState(GameState::PackOpening);
        } else if (inRect(ShopButton)) {
            game.setNextState(GameState::Payment);
        } else if (inRect(logoutButton)) {
            RenderBackdrop::resetElapsed();
            Audio::stopMusic();
            animInitialized = false;
            game.endUserSession();
            game.getNetworkClient().disconnect();
            game.setPlayerUsername("Player");
            game.setNextState(GameState::Login);
        } else if (inRect(settingsButton)) {
            settingsOpen = true;
        } else if (inRect(quitButton)) {
            game.setNextState(GameState::Quit);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// update
// ─────────────────────────────────────────────────────────────────────────────
void Title::update(Game& game) {
    int mouseX, mouseY;
    SDL_GetMouseState(&mouseX, &mouseY);

    hoveredButton = -1;
    if (settingsOpen) return; // no button hover while modal is open

    auto checkHover = [&](const SDL_Rect& r, int idx) {
        if (mouseX >= r.x && mouseX <= r.x + r.w &&
            mouseY >= r.y && mouseY <= r.y + r.h) {
            hoveredButton = idx;
        }
    };

    checkHover(startButton,     0);
    checkHover(BuildDeckButton, 1);
    checkHover(OpenPacksButton, 2);
    checkHover(ShopButton,      3);
    checkHover(logoutButton,    4);
    checkHover(settingsButton,  5);
    checkHover(quitButton,      6);
}

// ─────────────────────────────────────────────────────────────────────────────
// renderSlider  —  draws one labelled slider inside the settings modal
// ─────────────────────────────────────────────────────────────────────────────
void Title::renderSlider(SDL_Renderer*   renderer,
                         TTF_Font*       labelFont,
                         RenderText&     textRenderer,
                         const SDL_Rect& track,
                         int             volume,
                         const std::string& label) const
{
    // Label + percentage, e.g.  "Music Volume   89%"
    const std::string pct    = std::to_string(static_cast<int>(volume / 1.28f)) + "%";
    const std::string fullLabel = label + "   " + pct;

    const int labelGap = static_cast<int>(6 * scale);
    const int labelY   = track.y - static_cast<int>(26 * scale);

    if (labelFont) {
        int lw = 0, lh = 0;
        TTF_SizeText(labelFont, fullLabel.c_str(), &lw, &lh);
        // Left-align with track
        textRenderer.drawText(renderer, fullLabel, labelFont,
                              Theme::TEXT_IVORY, track.x, labelY);
    }

    // Track background
    const int cornerR = track.h / 2;
    RenderUtil::drawRoundedRect(renderer, track, cornerR,
                                SDL_Color{40, 40, 60, 200},
                                SDL_Color{80, 80, 110, 220});

    // Filled portion
    const float ratio   = std::clamp(static_cast<float>(volume) / 128.0f, 0.0f, 1.0f);
    const int   fillW   = static_cast<int>(track.w * ratio);
    if (fillW > 0) {
        SDL_Rect fill{track.x, track.y, fillW, track.h};
        RenderUtil::drawRoundedRect(renderer, fill, cornerR,
                                    Theme::BTN_PRIMARY,
                                    SDL_Color{0, 0, 0, 0});
    }

    // Thumb (circle-ish square)
    const int thumbSz  = static_cast<int>(track.h * 1.8f);
    const int thumbX   = track.x + fillW - thumbSz / 2;
    const int thumbY   = track.y + (track.h - thumbSz) / 2;
    SDL_Rect  thumb    = {thumbX, thumbY, thumbSz, thumbSz};
    RenderUtil::drawRoundedRect(renderer, thumb, thumbSz / 2,
                                Theme::BTN_PRIMARY,
                                Theme::BTN_BORDER);
}

// ─────────────────────────────────────────────────────────────────────────────
// render
// ─────────────────────────────────────────────────────────────────────────────
void Title::render(Game& game) {
    SDL_Renderer*              renderer   = game.getRenderer();
    const RenderText::FontSet& titleFonts = game.getTitleFonts();
    const RenderText::FontSet& uiFonts    = game.getUIFonts();

    updateLayout(renderer);

    int screenW, screenH;
    SDL_GetRendererOutputSize(renderer, &screenW, &screenH);

    const bool animationsEnabled = AnimationFlag::getAnimationsEnabled();

    // ── animation timer ──────────────────────────────────────────────
    float elapsed = 999.0f;
    if (animationsEnabled) {
        if (!animInitialized) {
            animStartTick   = SDL_GetTicks();
            animInitialized = true;
        }
        elapsed = (SDL_GetTicks() - animStartTick) / 1000.0f;
    }

    // ── background + vignette ────────────────────────────────────────
    if (animationsEnabled) {
        RenderBackdrop::drawTitleBackdrop(renderer, screenW, screenH);
    } else {
        RenderBackdrop::drawBackgroundWithVignette(
            renderer, screenW, screenH,
            Theme::BG,
            SDL_Color{0, 0, 0, 255},
            80, 1.5f, 120
        );
    }

    // ── banner animation ─────────────────────────────────────────────
    float bannerT     = std::min(elapsed / 0.6f, 1.0f);
    float bannerEase  = 1.0f - (1.0f - bannerT) * (1.0f - bannerT);
    Uint8 bannerAlpha = static_cast<Uint8>(bannerEase * 255);
    int   bannerOffY  = static_cast<int>((1.0f - bannerEase) * -40);

    SDL_Rect  animBanner     = {titleBanner.x, titleBanner.y + bannerOffY,
                                titleBanner.w, titleBanner.h};
    SDL_Color animBannerFill = {Theme::BANNER_FILL.r,   Theme::BANNER_FILL.g,
                                Theme::BANNER_FILL.b,   bannerAlpha};
    SDL_Color animGold       = {Theme::BANNER_BORDER.r, Theme::BANNER_BORDER.g,
                                Theme::BANNER_BORDER.b, bannerAlpha};
    SDL_Color animText       = {Theme::BANNER_TEXT.r,   Theme::BANNER_TEXT.g,
                                Theme::BANNER_TEXT.b,   bannerAlpha};
    SDL_Color animGlow       = {Theme::BANNER_GLOW.r,   Theme::BANNER_GLOW.g,
                                Theme::BANNER_GLOW.b,   bannerAlpha};

    if (bannerAlpha > 0) {
        RenderBanner::drawBanner(renderer, animBanner, "Archcast",
                                 titleFonts.large, animBannerFill, animGold,
                                 animText, animGlow);
    }

    // ── button slide-in animation helper ─────────────────────────────
    auto buttonSlide = [&](int index) -> std::pair<Uint8, int> {
        float delay = 0.3f + index * 0.15f;
        float t     = std::min(std::max((elapsed - delay) / 0.5f, 0.0f), 1.0f);
        float ease  = 1.0f - (1.0f - t) * (1.0f - t);
        return {static_cast<Uint8>(ease * 255), static_cast<int>((1.0f - ease) * -40)};
    };

    auto drawBtn = [&](const SDL_Rect& rect, SDL_Color fill,
                       const std::string& text, int index)
    {
        auto [alpha, offY] = buttonSlide(index);
        if (alpha == 0) return;

        bool isHovered = (hoveredButton == index);
        auto& cache    = cachedButtons[static_cast<std::size_t>(index)];

        if (!cache.texture      ||
            cache.w       != rect.w    ||
            cache.h       != rect.h    ||
            cache.label   != text      ||
            cache.hovered != isHovered ||
            cache.lastScale != scale)
        {
            if (cache.texture) SDL_DestroyTexture(cache.texture);

            const int sOff = Theme::Effects::SHADOW_OFFSET;

            SDL_Texture* tex = SDL_CreateTexture(
                renderer,
                SDL_PIXELFORMAT_RGBA8888,
                SDL_TEXTUREACCESS_TARGET,
                rect.w + sOff,
                rect.h + sOff
            );
            if (!tex) return;

            SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
            SDL_SetRenderTarget(renderer, tex);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
            SDL_RenderClear(renderer);

            SDL_Rect shadowLocal{sOff, sOff, rect.w, rect.h};
            RenderUtil::drawRoundedShadow(renderer, shadowLocal,
                Theme::BTN_RADIUS, 0, Theme::Effects::SHADOW_COLOR);

            SDL_Rect local{0, 0, rect.w, rect.h};
            RenderButton::Style style{};
            style.fill       = fill;
            style.border     = Theme::BTN_BORDER;
            style.text       = Theme::BTN_TEXT;
            style.drawShadow = false;

            RenderButton::drawButton(renderer, local, text, uiFonts.large, style, isHovered);

            SDL_SetRenderTarget(renderer, nullptr);

            cache.texture   = tex;
            cache.w         = rect.w;
            cache.h         = rect.h;
            cache.label     = text;
            cache.hovered   = isHovered;
            cache.lastScale = scale;
        }

        if (!cache.texture) return;

        const int sOff = Theme::Effects::SHADOW_OFFSET;
        SDL_SetTextureAlphaMod(cache.texture, alpha);
        SDL_Rect dst{rect.x, rect.y + offY, rect.w + sOff, rect.h + sOff};
        SDL_RenderCopy(renderer, cache.texture, nullptr, &dst);
    };

    // Draw all seven buttons
    // Indices must match cachedButtons and hoveredButton checks in update()
    drawBtn(startButton,     Theme::BTN_START,     "Start Game", 0);
    drawBtn(BuildDeckButton, Theme::BTN_BUILD,      "Build Deck", 1);
    drawBtn(OpenPacksButton, Theme::BTN_PACKS,      "Open Packs", 2);
    drawBtn(ShopButton,      Theme::BTN_PRIMARY,    "Coin Shop",  3);
    drawBtn(logoutButton,    Theme::BTN_CONNECT,    "Logout",     4);
    drawBtn(settingsButton,  Theme::BTN_SECONDARY,  "Settings",   5);
    drawBtn(quitButton,      Theme::BTN_QUIT,       "Quit Game",  6);

    // ── Settings modal ───────────────────────────────────────────────
    if (settingsOpen) {
        // Dim overlay
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
        SDL_RenderFillRect(renderer, nullptr);

        // Modal panel
        RenderUtil::drawRoundedRect(renderer, settingsModal,
                                    static_cast<int>(14 * scale),
                                    Theme::PANEL_FILL,
                                    Theme::BTN_BORDER);

        // ── Title ─────────────────────────────────────────────────
        const char* modalTitle = "Settings";
        int titleW = 0, titleH = 0;
        if (titleFonts.medium) {
            TTF_SizeText(titleFonts.medium, modalTitle, &titleW, &titleH);
        }
        RenderText textRenderer;
        textRenderer.drawText(renderer, modalTitle, titleFonts.medium,
                              Theme::TEXT_IVORY,
                              settingsModal.x + (settingsModal.w - titleW) / 2,
                              settingsModal.y + static_cast<int>(20 * scale));

        // ── Close button  ( × ) ──────────────────────────────────
        int mouseX, mouseY;
        SDL_GetMouseState(&mouseX, &mouseY);
        const bool hoverClose = RenderUtil::pointInRect(settingsCloseButton, mouseX, mouseY);

        RenderButton::Style closeStyle{};
        closeStyle.fill   = hoverClose ? Theme::BTN_QUIT : Theme::BTN_SECONDARY;
        closeStyle.border = Theme::BTN_BORDER;
        closeStyle.text   = Theme::BTN_TEXT;
        closeStyle.radius = settingsCloseButton.w / 2;
        RenderButton::drawButton(renderer, settingsCloseButton, "X",
                                 uiFonts.medium, closeStyle, hoverClose, false);

        // ── Volume sliders ───────────────────────────────────────
        renderSlider(renderer, uiFonts.medium, textRenderer,
                     musicSliderTrack, Audio::getMusicVolume(), "Music Volume");

        renderSlider(renderer, uiFonts.medium, textRenderer,
                     sfxSliderTrack,   Audio::getSFXVolume(),   "SFX Volume");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// ~Title
// ─────────────────────────────────────────────────────────────────────────────
Title::~Title() {
    for (auto& cache : cachedButtons) {
        if (cache.texture) {
            SDL_DestroyTexture(cache.texture);
            cache.texture = nullptr;
        }
    }
}