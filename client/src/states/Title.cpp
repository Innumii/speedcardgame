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

namespace {
    float scale = 1.0f;
}

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
        renderer,
        local,
        text,
        fonts.large,
        fill,
        Theme::BTN_BORDER,
        Theme::BTN_TEXT,
        hovered
    );

    SDL_SetRenderTarget(renderer, nullptr);
    return tex;
}

static constexpr float kRefW = 1200.0F;
static constexpr float kRefH = 850.0F;

void Title::updateLayout(SDL_Renderer* renderer) {
    int screenW = 800;
    int screenH = 600;
    if (renderer) {
        SDL_GetRendererOutputSize(renderer, &screenW, &screenH);
    }

    scale = std::min(
        static_cast<float>(screenW) / kRefW,
        static_cast<float>(screenH) / kRefH);
    

    // Scale all button and banner dimensions from theme constants
    const int mainBtnW   = static_cast<int>(Theme::Title::MAIN_BUTTON_WIDTH   * scale);
    const int mainBtnH   = static_cast<int>(Theme::Title::MAIN_BUTTON_HEIGHT  * scale);
    const int smallBtnW  = static_cast<int>(Theme::Title::SMALL_BUTTON_WIDTH  * scale);
    const int smallBtnH  = static_cast<int>(Theme::Title::SMALL_BUTTON_HEIGHT * scale);
    const int bannerW    = static_cast<int>(Theme::Title::BANNER_WIDTH         * scale);
    const int bannerH    = static_cast<int>(Theme::Title::BANNER_HEIGHT        * scale);
    const int bannerGap  = static_cast<int>(24 * scale);
    const int buttonGap  = static_cast<int>(16 * scale);
    const int smallRowGap = static_cast<int>(12 * scale);

    startButton.w = mainBtnW;
    startButton.h = mainBtnH;
    BuildDeckButton.w = mainBtnW;
    BuildDeckButton.h = mainBtnH;
    OpenPacksButton.w = mainBtnW;
    OpenPacksButton.h = mainBtnH;
    ShopButton.w = mainBtnW;
    ShopButton.h = mainBtnH;
    logoutButton.w = smallBtnW;
    logoutButton.h = smallBtnH;
    quitButton.w = smallBtnW;
    quitButton.h = smallBtnH;
    titleBanner.w = bannerW;
    titleBanner.h = bannerH;

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

    const int smallTotalW = logoutButton.w + smallRowGap + quitButton.w;
    const int smallStartX = centerX - (smallTotalW / 2);
    const int smallY = ShopButton.y + mainBtnH + buttonGap;
    logoutButton.x = smallStartX;
    logoutButton.y = smallY;
    quitButton.x = logoutButton.x + logoutButton.w + smallRowGap;
    quitButton.y = smallY;
}

void Title::handleEvents(Game& game, const SDL_Event& event) {
    updateLayout(game.getRenderer());

    if (event.type == SDL_QUIT) {
        game.setNextState(GameState::Quit);
    }

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        int mouseX = event.button.x;
        int mouseY = event.button.y;

        const bool inStart     = (mouseX >= startButton.x     && mouseX <= startButton.x     + startButton.w) &&
                                 (mouseY >= startButton.y     && mouseY <= startButton.y     + startButton.h);
        const bool inBuildDeck = (mouseX >= BuildDeckButton.x && mouseX <= BuildDeckButton.x + BuildDeckButton.w) &&
                                 (mouseY >= BuildDeckButton.y && mouseY <= BuildDeckButton.y + BuildDeckButton.h);
        const bool inOpenPacks = (mouseX >= OpenPacksButton.x && mouseX <= OpenPacksButton.x + OpenPacksButton.w) &&
                     (mouseY >= OpenPacksButton.y && mouseY <= OpenPacksButton.y + OpenPacksButton.h);
        const bool inShop      = (mouseX >= ShopButton.x      && mouseX <= ShopButton.x      + ShopButton.w) &&
                     (mouseY >= ShopButton.y      && mouseY <= ShopButton.y      + ShopButton.h);
        const bool inLogout    = (mouseX >= logoutButton.x    && mouseX <= logoutButton.x    + logoutButton.w) &&
                                 (mouseY >= logoutButton.y    && mouseY <= logoutButton.y    + logoutButton.h);
        const bool inQuit      = (mouseX >= quitButton.x      && mouseX <= quitButton.x      + quitButton.w)  &&
                                 (mouseY >= quitButton.y      && mouseY <= quitButton.y      + quitButton.h);

        if (inStart) {
            game.setNextState(GameState::Connecting);
        } else if (inBuildDeck) {
            game.setNextState(GameState::DeckBuilding);
        } else if (inOpenPacks) {
            game.setNextState(GameState::PackOpening);
        } else if (inShop) {
            game.setNextState(GameState::Payment);
        } else if (inLogout) {
            animInitialized = false;
            game.endUserSession();
            game.getNetworkClient().disconnect();
            game.setPlayerUsername("Player");
            game.setNextState(GameState::Login);
        } else if (inQuit) {
            game.setNextState(GameState::Quit);
        }
    }
}

void Title::update(Game& game) {
    int mouseX, mouseY;
    SDL_GetMouseState(&mouseX, &mouseY);

    hoveredButton = -1;

    if (mouseX >= startButton.x && mouseX <= startButton.x + startButton.w &&
        mouseY >= startButton.y && mouseY <= startButton.y + startButton.h) {
        hoveredButton = 0;
    } else if (mouseX >= BuildDeckButton.x && mouseX <= BuildDeckButton.x + BuildDeckButton.w &&
               mouseY >= BuildDeckButton.y && mouseY <= BuildDeckButton.y + BuildDeckButton.h) {
        hoveredButton = 1;
    } else if (mouseX >= OpenPacksButton.x && mouseX <= OpenPacksButton.x + OpenPacksButton.w &&
               mouseY >= OpenPacksButton.y && mouseY <= OpenPacksButton.y + OpenPacksButton.h) {
        hoveredButton = 2;
    } else if (mouseX >= ShopButton.x && mouseX <= ShopButton.x + ShopButton.w &&
               mouseY >= ShopButton.y && mouseY <= ShopButton.y + ShopButton.h) {
        hoveredButton = 3;
    } else if (mouseX >= logoutButton.x && mouseX <= logoutButton.x + logoutButton.w &&
               mouseY >= logoutButton.y && mouseY <= logoutButton.y + logoutButton.h) {
        hoveredButton = 4;
    } else if (mouseX >= quitButton.x && mouseX <= quitButton.x + quitButton.w &&
               mouseY >= quitButton.y && mouseY <= quitButton.y + quitButton.h) {
        hoveredButton = 5;
    }
}

void Title::render(const Game& game) {
    SDL_Renderer*              renderer   = game.getRenderer();
    const RenderText::FontSet& titleFonts = game.getTitleFonts();
    const RenderText::FontSet& uiFonts    = game.getUIFonts();

    // Always recompute layout so it responds to window size changes
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
    RenderBackdrop::drawBackgroundWithVignette(
        renderer, screenW, screenH,
        Theme::BG,
        SDL_Color{0, 0, 0, 255},
        80, 1.5f, 120
    );

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

    // ── button animation ─────────────────────────────────────────────
    auto buttonSlide = [&](int index) -> std::pair<Uint8, int> {
        float delay = 0.3f + index * 0.15f;
        float t     = std::min(std::max((elapsed - delay) / 0.5f, 0.0f), 1.0f);
        float ease  = 1.0f - (1.0f - t) * (1.0f - t);
        return {static_cast<Uint8>(ease * 255), static_cast<int>((1.0f - ease) * -40)};
    };

    auto drawBtn = [&](const SDL_Rect& rect, SDL_Color fill,
                    const std::string& text, int index) {

        auto [alpha, offY] = buttonSlide(index);
        if (alpha == 0) return;

        SDL_Rect animRect = {rect.x, rect.y + offY, rect.w, rect.h};

        bool isHovered = (hoveredButton == index);
        auto& cache = cachedButtons[index];

        if (!cache.texture ||
            cache.w != rect.w ||
            cache.h != rect.h ||
            cache.label != text ||
            cache.hovered != isHovered ||
            cache.lastScale != scale)
        {
            if (cache.texture) {
                SDL_DestroyTexture(cache.texture);
            }

            cache.texture = buildButtonTexture(
                renderer, rect, text, fill, isHovered, uiFonts
            );

            cache.w = rect.w;
            cache.h = rect.h;
            cache.label = text;
            cache.hovered = isHovered;
        }

        if (!cache.texture) return;

        SDL_SetTextureAlphaMod(cache.texture, alpha);
        SDL_RenderCopy(renderer, cache.texture, nullptr, &animRect);
    };

    drawBtn(startButton,     Theme::BTN_START,   "Start Game", 0);
    drawBtn(BuildDeckButton, Theme::BTN_BUILD,   "Build Deck", 1);
    drawBtn(OpenPacksButton, Theme::BTN_START,   "Open Packs", 2);
    drawBtn(ShopButton,      Theme::BTN_PRIMARY, "Coin Shop",  3);
    drawBtn(logoutButton,    Theme::BTN_CONNECT, "Logout",     4);
    drawBtn(quitButton,      Theme::BTN_QUIT,    "Quit Game",  5);
}

Title::~Title() {
    for (auto& cache : cachedButtons) {
        if (cache.texture) {
            SDL_DestroyTexture(cache.texture);
            cache.texture = nullptr;
        }
    }
}