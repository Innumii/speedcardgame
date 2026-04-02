#include "states/Title.hpp"
#include "core/Game.hpp"
#include "featureFlag/AnimationFlag.hpp"
#include "render/RenderBackdrop.hpp"
#include "render/RenderText.hpp"
#include "render/RenderButton.hpp"
#include "render/RenderMenuButton.hpp"
#include "render/RenderCard.hpp"
#include "render/Theme.hpp"
#include "objects/Deck.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
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

    // Reshuffle the deck and reset conveyor state.  Card pre-population is
    // deferred to the first renderShowcase() call so layout dims are known.
    prepareShowcaseDeck(game);
}

// ─────────────────────────────────────────────────────────────────────────────
// Layout constants
// ─────────────────────────────────────────────────────────────────────────────
static constexpr float kRefW = 1200.0f;
static constexpr float kRefH = 850.0f;

// ─────────────────────────────────────────────────────────────────────────────
// updateLayout
// ─────────────────────────────────────────────────────────────────────────────
void Title::updateLayout(SDL_Renderer* renderer,
                         TTF_Font*     titleFont,
                         TTF_Font*     menuFont)
{
    int screenW = 800, screenH = 600;
    if (renderer) SDL_GetRendererOutputSize(renderer, &screenW, &screenH);

    scale = std::min(
        static_cast<float>(screenW) / kRefW,
        static_cast<float>(screenH) / kRefH);

    const int padX     = static_cast<int>(70  * scale);
    const int padY     = static_cast<int>(55  * scale);
    const int indent   = static_cast<int>(22  * scale);
    const int titleGap = static_cast<int>(44  * scale);
    const int btnGap   = static_cast<int>(16  * scale);
    const int groupGap = static_cast<int>(30  * scale);

    titlePos = {padX, padY};

    int titleH = static_cast<int>(64 * scale);
    if (titleFont) {
        int tw = 0;
        TTF_SizeText(titleFont, "Archcast", &tw, &titleH);
    }

    const int btnX = padX + indent;
    int       btnY = padY + titleH + titleGap;

    auto nextRect = [&](const std::string& label, int extraGap = 0) -> SDL_Rect {
        btnY += extraGap;
        SDL_Rect r = RenderMenuButton::measure(menuFont, label, btnX, btnY);
        if (r.h < 4) r.h = static_cast<int>(32 * scale);
        btnY += r.h + btnGap;
        return r;
    };

    startButton     = nextRect("Start Game");
    BuildDeckButton = nextRect("Build Deck");
    OpenPacksButton = nextRect("Open Packs");
    ShopButton      = nextRect("Coin Shop");
    settingsButton  = nextRect("Settings",  groupGap);
    logoutButton    = nextRect("Logout");
    quitButton      = nextRect("Quit Game");

    buttonAreaRight = std::max({
        startButton.x     + startButton.w,
        BuildDeckButton.x + BuildDeckButton.w,
        OpenPacksButton.x + OpenPacksButton.w,
        ShopButton.x      + ShopButton.w,
        settingsButton.x  + settingsButton.w,
        logoutButton.x    + logoutButton.w,
        quitButton.x      + quitButton.w
    });

    buttonGroupCenterY = (startButton.y + quitButton.y + quitButton.h) / 2;

    updateSettingsLayout(screenW, screenH);
}

// ─────────────────────────────────────────────────────────────────────────────
// updateSettingsLayout
// ─────────────────────────────────────────────────────────────────────────────
void Title::updateSettingsLayout(int screenW, int screenH) {
    const int modalW = static_cast<int>(480 * scale);
    const int modalH = static_cast<int>(280 * scale);
    settingsModal = {(screenW - modalW) / 2, (screenH - modalH) / 2, modalW, modalH};

    const int trackH    = static_cast<int>(14 * scale);
    const int trackPadX = static_cast<int>(40 * scale);
    const int trackW    = modalW - (trackPadX * 2);
    const int labelRowH = static_cast<int>(28 * scale);

    const int firstY = settingsModal.y + static_cast<int>(90 * scale);
    musicSliderTrack = {settingsModal.x + trackPadX, firstY, trackW, trackH};

    const int secondY = firstY + labelRowH + trackH + static_cast<int>(40 * scale);
    sfxSliderTrack   = {settingsModal.x + trackPadX, secondY, trackW, trackH};

    const int closeBtnW = static_cast<int>(120 * scale);
    const int closeBtnH = static_cast<int>(36  * scale);
    settingsCloseButton = {
        settingsModal.x + (settingsModal.w - closeBtnW) / 2,
        settingsModal.y + settingsModal.h - closeBtnH - static_cast<int>(16 * scale),
        closeBtnW, closeBtnH
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
    const auto& tFonts = game.getTitleFonts();
    updateLayout(game.getRenderer(), tFonts.large, tFonts.medium);

    if (event.type == SDL_QUIT) {
        game.setNextState(GameState::Quit);
        return;
    }

    if (settingsOpen) {
        switch (event.type) {
            case SDL_MOUSEBUTTONDOWN: {
                if (event.button.button != SDL_BUTTON_LEFT) break;
                const int mx = event.button.x, my = event.button.y;

                if (RenderUtil::pointInRect(settingsCloseButton, mx, my)) {
                    settingsOpen = draggingMusicSlider = draggingSFXSlider = false;
                    break;
                }
                if (RenderUtil::pointInRect(musicSliderTrack, mx, my)) {
                    Audio::setMusicVolume(sliderVolumeFromMouseX(musicSliderTrack, mx));
                    draggingMusicSlider = true;
                    break;
                }
                if (RenderUtil::pointInRect(sfxSliderTrack, mx, my)) {
                    Audio::setSFXVolume(sliderVolumeFromMouseX(sfxSliderTrack, mx));
                    draggingSFXSlider = true;
                    break;
                }
                if (!RenderUtil::pointInRect(settingsModal, mx, my))
                    settingsOpen = false;
                break;
            }
            case SDL_MOUSEMOTION:
                if (draggingMusicSlider)
                    Audio::setMusicVolume(sliderVolumeFromMouseX(musicSliderTrack, event.motion.x));
                else if (draggingSFXSlider)
                    Audio::setSFXVolume(sliderVolumeFromMouseX(sfxSliderTrack, event.motion.x));
                break;
            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT)
                    draggingMusicSlider = draggingSFXSlider = false;
                break;
            default: break;
        }
        return;
    }

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        const int mx = event.button.x, my = event.button.y;

        auto hit = [&](const SDL_Rect& r) {
            return mx >= r.x && mx <= r.x + r.w &&
                   my >= r.y && my <= r.y + r.h;
        };

        if      (hit(startButton))     game.setNextState(GameState::Connecting);
        else if (hit(BuildDeckButton)) game.setNextState(GameState::DeckBuilding);
        else if (hit(OpenPacksButton)) game.setNextState(GameState::PackOpening);
        else if (hit(ShopButton))      game.setNextState(GameState::Payment);
        else if (hit(settingsButton))  settingsOpen = true;
        else if (hit(logoutButton)) {
            RenderBackdrop::resetElapsed();
            Audio::stopMusic();
            animInitialized = false;
            showcase.active.clear();
            showcase.deckReady  = false;
            showcase.prePopDone = false;
            cardTexCache_.clear();
            cachedTexW_ = cachedTexH_ = 0;
            game.endUserSession();
            game.getNetworkClient().disconnect();
            game.setPlayerUsername("Player");
            game.setNextState(GameState::Login);
        }
        else if (hit(quitButton))      game.setNextState(GameState::Quit);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// update
// ─────────────────────────────────────────────────────────────────────────────
void Title::update(Game& game) {
    int mx, my;
    SDL_GetMouseState(&mx, &my);

    hoveredButton = -1;
    if (settingsOpen) return;

    auto checkHover = [&](const SDL_Rect& r, int idx) {
        if (mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h)
            hoveredButton = idx;
    };

    checkHover(startButton,     0);
    checkHover(BuildDeckButton, 1);
    checkHover(OpenPacksButton, 2);
    checkHover(ShopButton,      3);
    checkHover(settingsButton,  4);
    checkHover(logoutButton,    5);
    checkHover(quitButton,      6);
}

// ─────────────────────────────────────────────────────────────────────────────
// renderSlider
// ─────────────────────────────────────────────────────────────────────────────
void Title::renderSlider(SDL_Renderer*      renderer,
                         TTF_Font*          labelFont,
                         RenderText&        textRenderer,
                         const SDL_Rect&    track,
                         int                volume,
                         const std::string& label) const
{
    const std::string pct      = std::to_string(static_cast<int>(volume / 1.28f)) + "%";
    const std::string fullLabel = label + "   " + pct;

    if (labelFont)
        textRenderer.drawText(renderer, fullLabel, labelFont,
                              Theme::TEXT_IVORY,
                              track.x,
                              track.y - static_cast<int>(26 * scale));

    const int cornerR = track.h / 2;
    RenderUtil::drawRoundedRect(renderer, track, cornerR,
                                SDL_Color{40, 40, 60, 200},
                                SDL_Color{80, 80, 110, 220});

    const float ratio = std::clamp(static_cast<float>(volume) / 128.0f, 0.0f, 1.0f);
    const int   fillW = static_cast<int>(track.w * ratio);
    if (fillW > 0) {
        SDL_Rect fill{track.x, track.y, fillW, track.h};
        RenderUtil::drawRoundedRect(renderer, fill, cornerR,
                                    Theme::BTN_PRIMARY, SDL_Color{0, 0, 0, 0});
    }

    const int thumbSz = static_cast<int>(track.h * 1.8f);
    SDL_Rect  thumb   = {track.x + fillW - thumbSz / 2,
                         track.y + (track.h - thumbSz) / 2,
                         thumbSz, thumbSz};
    RenderUtil::drawRoundedRect(renderer, thumb, thumbSz / 4,
                                Theme::BTN_PRIMARY, Theme::BTN_BORDER);
}

// ─────────────────────────────────────────────────────────────────────────────
// prepareShowcaseDeck
// ─────────────────────────────────────────────────────────────────────────────
void Title::prepareShowcaseDeck(Game& game) {
    showcase.deck.clear();
    showcase.cardLookup.clear();   // ← ADD THIS
    showcase.nextIdx       = 0;
    showcase.active.clear();
    showcase.animTime      = 0.0f;
    showcase.lastTick      = 0;
    showcase.lastSpawnTime = 0.0f;
    showcase.deckReady     = false;
    showcase.prePopDone    = false;

    const Deck& gameDeck = game.getDeck();
    if (!gameDeck.isEmpty()) {
        Deck copy = gameDeck.clone();
        copy.shuffle();
        while (!copy.isEmpty()) {
            auto uniqueCard = copy.draw();
            auto sharedCard = std::shared_ptr<Card>(std::move(uniqueCard));

            showcase.cardLookup[sharedCard->getId()] = sharedCard;
            showcase.deck.push_back(sharedCard);
        }
    }

    showcase.deckReady = true;
}

// ─────────────────────────────────────────────────────────────────────────────
// getOrBuildCardTex
//
// Returns the CachedCardTex entry for cardId, building it first if absent.
// Pass cardId = -1 for the shared card-back-only entry (front will be null).
//
// Cache invalidation: if the caller detects a dimension change it must clear
// cardTexCache_ and reset cachedTexW_/cachedTexH_ before calling here.
// ─────────────────────────────────────────────────────────────────────────────
Title::CachedCardTex& Title::getOrBuildCardTex(SDL_Renderer*                renderer,
                                                Game&                        game,
                                                int                          cardId,
                                                const std::shared_ptr<Card>& card,
                                                int                          texW,
                                                int                          texH)
{
    auto it = cardTexCache_.find(cardId);
    if (it != cardTexCache_.end())
        return it->second;

    // Entry not found — build it now.
    CachedCardTex entry;

    if (!renderer || !SDL_RenderTargetSupported(renderer)) {
        cardTexCache_.emplace(cardId, std::move(entry));
        return cardTexCache_.at(cardId);
    }

    SDL_Texture* const prevTarget = SDL_GetRenderTarget(renderer);

    auto makeTarget = [&]() -> SDL_Texture* {
        SDL_Texture* t = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                           SDL_TEXTUREACCESS_TARGET, texW, texH);
        if (t) SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
        return t;
    };

    auto clearTarget = [&](SDL_Texture* t) {
        SDL_SetRenderTarget(renderer, t);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    };

    // ── Front face (only for real cards; not the back-only sentinel) ──
    if (card && cardId != -1) {
        if (SDL_Texture* ft = makeTarget()) {
            clearTarget(ft);
            RenderText dummy;
            const SDL_Rect cardRect{0, 0, texW, texH};
            RenderCard::drawHandCard(renderer, dummy, *card, cardRect,
                                     game.getUIFonts().large,
                                     game.getUIFonts().medium);
            entry.front = TexPtr(ft, SDL_DestroyTexture);
        }
    }

    // ── Card back ─────────────────────────────────────────────────────
    // Stored in every entry so each lookup has both textures without a
    // second map hit.  The texture itself is built fresh per entry, but
    // it's trivially cheap (no card-specific data).
    if (SDL_Texture* bt = makeTarget()) {
        clearTarget(bt);
        RenderCard::drawCardBack(renderer, {0, 0, texW, texH});
        entry.back = TexPtr(bt, SDL_DestroyTexture);
    }

    SDL_SetRenderTarget(renderer, prevTarget);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    auto [ins, _] = cardTexCache_.emplace(cardId, std::move(entry));
    return ins->second;
}

// ─────────────────────────────────────────────────────────────────────────────
// renderShowcase
//
// Conveyor model  (unchanged from original — only texture and shading code
// has been modified)
// ─────────────────────────────────────────────────────────────────────────────
void Title::renderShowcase(SDL_Renderer* renderer, Game& game,
                           int screenW, int screenH, float elapsed)
{
    // ── Advance animation clock ───────────────────────────────────────
    const Uint32 now = SDL_GetTicks();
    if (showcase.lastTick == 0) showcase.lastTick = now;
    showcase.animTime += static_cast<float>(now - showcase.lastTick) / 1000.0f;
    showcase.lastTick  = now;

    if (!showcase.deckReady) return;

    const int texW = static_cast<int>(kShowcaseW * scale);
    const int texH = static_cast<int>(kShowcaseH * scale);

    // ── Invalidate texture cache on dimension change (window resize) ──
    if (texW != cachedTexW_ || texH != cachedTexH_) {
        cardTexCache_.clear();
        cachedTexW_ = texW;
        cachedTexH_ = texH;
    }

    // ── Conveyor X bounds (card centres) ─────────────────────────────
    const float leftX  = static_cast<float>(buttonAreaRight)
                         + static_cast<float>(texW) * 0.5f
                         + 40.0f * scale;
    const float rightX = static_cast<float>(screenW) + texW * 0.6f;

    const float travelDist = rightX - leftX;
    if (travelDist <= 0.0f) return;

    // ── Derived conveyor values ───────────────────────────────────────
    const float speed         = kCardSpeedRef * scale;
    const float gapPx         = kCardGapRef   * scale;
    const float spawnInterval = kCardGapRef   / kCardSpeedRef;
    const float cardLifetime  = travelDist    / speed;

    // ── Pre-populate on first call ────────────────────────────────────
    if (!showcase.prePopDone) {
        const int numCards = static_cast<int>(travelDist / gapPx) + 2;
        for (int i = 0; i < numCards; ++i) {
            ActiveCard ac;
            if (!showcase.deck.empty()) {
                ac.cardId = showcase.deck[showcase.nextIdx % showcase.deck.size()]->getId();
                showcase.nextIdx++;
            }
            ac.birthTime  = -static_cast<float>(i) * spawnInterval;
            ac.flipOffset = static_cast<float>(i) * 2.1f;
            showcase.active.push_back(std::move(ac));
        }
        showcase.lastSpawnTime = 0.0f;
        showcase.prePopDone    = true;
    }

    // ── Spawn one new card each interval ─────────────────────────────
    if (showcase.animTime - showcase.lastSpawnTime >= spawnInterval) {
        ActiveCard ac;
        if (!showcase.deck.empty()) {
            ac.cardId = showcase.deck[showcase.nextIdx % showcase.deck.size()]->getId();
            showcase.nextIdx++;
        }
        ac.birthTime  = showcase.animTime;
        ac.flipOffset = static_cast<float>(showcase.nextIdx) * 1.3f;
        showcase.active.push_back(std::move(ac));
        showcase.lastSpawnTime = showcase.animTime;
    }

    // ── Retire cards that have fully crossed to the left ─────────────
    showcase.active.erase(
        std::remove_if(showcase.active.begin(), showcase.active.end(),
            [&](const ActiveCard& ac) {
                return (showcase.animTime - ac.birthTime) >= cardLifetime;
            }),
        showcase.active.end());

    // ── Global fade-in ────────────────────────────────────────────────
    const float fadeDelay  = 0.55f;
    const float fadeDur    = 0.50f;
    const float fadeT      = std::clamp((elapsed - fadeDelay) / fadeDur, 0.0f, 1.0f);
    const float globalFade = 1.0f - (1.0f - fadeT) * (1.0f - fadeT);
    if (globalFade < 0.01f) return;

    // ── Render — oldest card first (back-to-front) ────────────────────
    for (auto& ac : showcase.active) {
        const float age   = showcase.animTime - ac.birthTime;
        const float phase = (age * speed) / travelDist;
        if (phase < 0.0f || phase >= 1.0f) continue;

        // ── Flip geometry ─────────────────────────────────────────────
        const float cosA       = std::cos(showcase.animTime * kFlipSpeed + ac.flipOffset);
        const bool  showFront  = (cosA >= 0.0f);
        const float widthFactor = std::abs(cosA);
        if (widthFactor < 0.01f) continue;

        // ── Look up (or build) cached textures ────────────────────────
        const int cardId = ac.cardId;
        auto card = getCardById(cardId);

        CachedCardTex& cached = getOrBuildCardTex(renderer, game,
                                                cardId, card,
                                                texW, texH);

        SDL_Texture* activeTex = showFront ? cached.front.get() : cached.back.get();
        if (!activeTex) activeTex = cached.back.get() ? cached.back.get()
                                                       : cached.front.get();
        if (!activeTex) continue;

        // ── Per-card alpha (fade in/out) × global fade ────────────────
        float cardAlpha = 1.0f;
        if (phase < kCardFadeInPhase)
            cardAlpha = phase / kCardFadeInPhase;
        else if (phase > kCardFadeOutPhase)
            cardAlpha = 1.0f - (phase - kCardFadeOutPhase)
                              / (1.0f - kCardFadeOutPhase);
        cardAlpha = std::clamp(cardAlpha * globalFade, 0.0f, 1.0f);
        if (cardAlpha < 0.01f) continue;

        // ── Card centre ───────────────────────────────────────────────
        const float cx = rightX + (leftX - rightX) * phase;
        const float floatOffY = std::sin(showcase.animTime * kFloatSpeed + ac.flipOffset)
                                * kFloatAmpRef * scale;
        const float cy = static_cast<float>(buttonGroupCenterY) + floatOffY;

        // ── Geometry: tilt + flip squish ──────────────────────────────
        const float halfW0 = texW * 0.5f;
        const float halfH0 = texH * 0.5f;

        constexpr float kPi = 3.14159265358979323846f;
        const float axisAngle = kFlipAxisAngleDeg * (kPi / 180.0f);
        const float cosT      = std::cos(axisAngle);
        const float sinT      = std::sin(axisAngle);

        auto rotate = [&](float lx, float ly) -> SDL_FPoint {
            return { cx + lx * cosT - ly * sinT,
                     cy + lx * sinT + ly * cosT };
        };

        SDL_FPoint A = rotate(-halfW0, -halfH0);
        SDL_FPoint B = rotate( halfW0, -halfH0);
        SDL_FPoint C = rotate( halfW0,  halfH0);
        SDL_FPoint D = rotate(-halfW0,  halfH0);

        const float axisX = std::sin(axisAngle);
        const float axisY = std::cos(axisAngle);
        for (SDL_FPoint* p : {&A, &B, &C, &D}) {
            const float px    = p->x - cx;
            const float py    = p->y - cy;
            const float dot   = px * axisX + py * axisY;
            const float parX  = dot * axisX;
            const float parY  = dot * axisY;
            const float perpX = px - parX;
            const float perpY = py - parY;
            p->x = cx + parX + perpX * widthFactor;
            p->y = cy + parY + perpY * widthFactor;
        }

        // ── Directional shading ───────────────────────────────────────
        //
        // Old code used widthFactor (abs(cosA)) as the brightness driver,
        // which is symmetrical: both the front and back face go dark at the
        // flip midpoint, making the card appear to vanish.
        //
        // New approach: measure how directly the *visible* face is turned
        // toward the viewer.
        //   cosA  +1 → front fully face-on  (bright)
        //   cosA   0 → edge-on              (kMinBrightness)
        //   cosA  -1 → back fully face-on   (bright)
        //
        // faceToViewer is always in [0, 1] for the face we are drawing,
        // so brightness climbs from kMinBrightness at the edge toward 1.0
        // as either face turns to face the camera.  The shadow therefore
        // favours the side of the card facing away, never both sides at once.
        constexpr float kMinBrightness = 0.52f;
        const float faceToViewer = showFront ? cosA : -cosA;  // 0..1
        const float bright = kMinBrightness
                             + (1.0f - kMinBrightness) * faceToViewer;
        const Uint8 b = static_cast<Uint8>(std::clamp(bright, 0.0f, 1.0f) * 255.0f);
        const Uint8 a = static_cast<Uint8>(cardAlpha * 255.0f);
        const SDL_Color col{b, b, b, a};

        // Mirror U when showing back so the texture reads correctly.
        const float u0 = showFront ? 0.0f : 1.0f;
        const float u1 = showFront ? 1.0f : 0.0f;

        const SDL_Vertex verts[4] = {
            {A, col, {u0, 0.0f}},
            {B, col, {u1, 0.0f}},
            {C, col, {u1, 1.0f}},
            {D, col, {u0, 1.0f}},
        };
        const int indices[6] = {0, 1, 2, 0, 2, 3};
        SDL_RenderGeometry(renderer, activeTex, verts, 4, indices, 6);
    }
}

std::shared_ptr<Card> Title::getCardById(int cardId) const {
    auto it = showcase.cardLookup.find(cardId);
    return (it != showcase.cardLookup.end()) ? it->second : nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// render
// ─────────────────────────────────────────────────────────────────────────────
void Title::render(Game& game) {
    SDL_Renderer*              renderer   = game.getRenderer();
    const RenderText::FontSet& titleFonts = game.getTitleFonts();
    const RenderText::FontSet& uiFonts    = game.getUIFonts();

    updateLayout(renderer, titleFonts.large, titleFonts.medium);

    int screenW, screenH;
    SDL_GetRendererOutputSize(renderer, &screenW, &screenH);

    const bool animationsEnabled = AnimationFlag::getAnimationsEnabled();

    float elapsed = 999.0f;
    if (animationsEnabled) {
        if (!animInitialized) {
            animStartTick   = SDL_GetTicks();
            animInitialized = true;
        }
        elapsed = (SDL_GetTicks() - animStartTick) / 1000.0f;
    }

    if (animationsEnabled)
        RenderBackdrop::drawTitleBackdrop(renderer, screenW, screenH);
    else
        RenderBackdrop::drawBackgroundWithVignette(
            renderer, screenW, screenH,
            Theme::BG, SDL_Color{0, 0, 0, 255},
            80, 1.5f, 120);

    // ── Title text with glow ──────────────────────────────────────────
    {
        const float t     = std::min(elapsed / 0.6f, 1.0f);
        const float ease  = 1.0f - (1.0f - t) * (1.0f - t);
        const Uint8 alpha = static_cast<Uint8>(ease * 255);
        const int   offY  = static_cast<int>((1.0f - ease) * -30);

        if (alpha > 0 && titleFonts.large) {
            SDL_Surface* surf = TTF_RenderText_Blended(
                titleFonts.large, "Archcast", {255, 255, 255, 255});
            if (surf) {
                SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                SDL_FreeSurface(surf);
                if (tex) {
                    int tw = 0, th = 0;
                    SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
                    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

                    struct GlowPass { int spread; Uint8 a; };
                    constexpr GlowPass passes[] = {{5, 12}, {3, 20}, {2, 30}};

                    SDL_SetTextureColorMod(tex,
                        Theme::BANNER_GLOW.r,
                        Theme::BANNER_GLOW.g,
                        Theme::BANNER_GLOW.b);

                    for (const auto& p : passes) {
                        SDL_SetTextureAlphaMod(tex,
                            static_cast<Uint8>(p.a * alpha / 255));
                        for (int dx = -p.spread; dx <= p.spread; dx += p.spread) {
                            for (int dy = -p.spread; dy <= p.spread; dy += p.spread) {
                                if (dx == 0 && dy == 0) continue;
                                SDL_Rect d{titlePos.x + dx,
                                           titlePos.y + offY + dy, tw, th};
                                SDL_RenderCopy(renderer, tex, nullptr, &d);
                            }
                        }
                    }

                    SDL_SetTextureColorMod(tex,
                        Theme::BANNER_TEXT.r,
                        Theme::BANNER_TEXT.g,
                        Theme::BANNER_TEXT.b);
                    SDL_SetTextureAlphaMod(tex, alpha);
                    SDL_Rect dst{titlePos.x, titlePos.y + offY, tw, th};
                    SDL_RenderCopy(renderer, tex, nullptr, &dst);

                    SDL_DestroyTexture(tex);
                }
            }
        }
    }

    // ── Menu buttons (staggered slide-in) ────────────────────────────
    auto drawMenuBtn = [&](const SDL_Rect& rect,
                           const std::string& label,
                           int index)
    {
        const float delay = 0.25f + index * 0.10f;
        const float t     = std::min(std::max((elapsed - delay) / 0.4f, 0.0f), 1.0f);
        const float ease  = 1.0f - (1.0f - t) * (1.0f - t);
        const Uint8 alpha = static_cast<Uint8>(ease * 255);
        const int   offY  = static_cast<int>((1.0f - ease) * -20);

        if (alpha == 0 || !titleFonts.medium) return;

        RenderMenuButton::draw(renderer,
                               rect.x, rect.y + offY,
                               label, titleFonts.medium,
                               hoveredButton == index,
                               alpha);
    };

    drawMenuBtn(startButton,     "Play",       0);
    drawMenuBtn(BuildDeckButton, "Deck",       1);
    drawMenuBtn(OpenPacksButton, "Open Packs", 2);
    drawMenuBtn(ShopButton,      "Coin Shop",  3);
    drawMenuBtn(settingsButton,  "Settings",   4);
    drawMenuBtn(logoutButton,    "Logout",     5);
    drawMenuBtn(quitButton,      "Quit Game",  6);

    // ── Showcase card conveyor ────────────────────────────────────────
    renderShowcase(renderer, game, screenW, screenH, elapsed);

    // ── Settings modal ────────────────────────────────────────────────
    if (settingsOpen) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
        SDL_RenderFillRect(renderer, nullptr);

        RenderUtil::drawRoundedRect(renderer, settingsModal,
                                    static_cast<int>(14 * scale),
                                    Theme::PANEL_FILL, Theme::BTN_BORDER);

        const char* modalTitle = "Settings";
        int titleW = 0, titleH = 0;
        if (titleFonts.medium)
            TTF_SizeText(titleFonts.medium, modalTitle, &titleW, &titleH);

        RenderText textRenderer;
        textRenderer.drawText(renderer, modalTitle, titleFonts.medium,
                              Theme::TEXT_IVORY,
                              settingsModal.x + (settingsModal.w - titleW) / 2,
                              settingsModal.y + static_cast<int>(20 * scale));

        int mouseX, mouseY;
        SDL_GetMouseState(&mouseX, &mouseY);
        const bool hoverClose = RenderUtil::pointInRect(settingsCloseButton, mouseX, mouseY);

        RenderButton::Style closeStyle{};
        closeStyle.fill   = hoverClose ? Theme::BTN_QUIT : Theme::BTN_SECONDARY;
        closeStyle.border = Theme::BTN_BORDER;
        closeStyle.text   = Theme::BTN_TEXT;
        closeStyle.radius = static_cast<int>(8 * scale);
        RenderButton::drawButton(renderer, settingsCloseButton, "Close",
                                 uiFonts.medium, closeStyle, hoverClose, false);

        renderSlider(renderer, uiFonts.medium, textRenderer,
                     musicSliderTrack, Audio::getMusicVolume(), "Music Volume");
        renderSlider(renderer, uiFonts.medium, textRenderer,
                     sfxSliderTrack,   Audio::getSFXVolume(),   "SFX Volume");
    }
}