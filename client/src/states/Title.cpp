#include "states/Title.hpp"
#include "core/Game.hpp"
#include "render/RenderText.hpp"
#include "render/RenderBanner.hpp"
#include "render/RenderButton.hpp"
#include "render/Theme.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <iostream>
#include <cmath>

void Title::updateLayout(SDL_Renderer* renderer) {
    int screenW = 800;
    int screenH = 600;
    if (renderer) {
        SDL_GetRendererOutputSize(renderer, &screenW, &screenH);
    }

    const int bannerW   = titleBanner.w;
    const int bannerH   = titleBanner.h;
    const int buttonW   = startButton.w;
    const int buttonH   = startButton.h;
    const int bannerGap = 24;
    const int buttonGap = 16;

    const int buttonsTotalH = (buttonH * 5) + (buttonGap * 4);
    const int totalH        = bannerH + bannerGap + buttonsTotalH;
    int topY = (screenH - totalH) / 2;
    if (topY < 20) topY = 20;

    const int centerX = screenW / 2;

    titleBanner.x     = centerX - (bannerW / 2);
    titleBanner.y     = topY;

    startButton.x     = centerX - (buttonW / 2);
    startButton.y     = titleBanner.y + bannerH + bannerGap;

    quitButton.x      = startButton.x;
    quitButton.y      = startButton.y + buttonH + buttonGap;

    BuildDeckButton.x = startButton.x;
    BuildDeckButton.y = quitButton.y + buttonH + buttonGap;

    OpenPacksButton.x = startButton.x;
    OpenPacksButton.y = BuildDeckButton.y + buttonH + buttonGap;

    ConnectButton.x   = startButton.x;
    ConnectButton.y   = OpenPacksButton.y + buttonH + buttonGap;
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
        const bool inQuit      = (mouseX >= quitButton.x      && mouseX <= quitButton.x      + quitButton.w)  &&
                                 (mouseY >= quitButton.y      && mouseY <= quitButton.y      + quitButton.h);
        const bool inBuildDeck = (mouseX >= BuildDeckButton.x && mouseX <= BuildDeckButton.x + BuildDeckButton.w) &&
                                 (mouseY >= BuildDeckButton.y && mouseY <= BuildDeckButton.y + BuildDeckButton.h);
        const bool inOpenPacks = (mouseX >= OpenPacksButton.x && mouseX <= OpenPacksButton.x + OpenPacksButton.w) &&
                     (mouseY >= OpenPacksButton.y && mouseY <= OpenPacksButton.y + OpenPacksButton.h);
        const bool inConnect   = (mouseX >= ConnectButton.x   && mouseX <= ConnectButton.x   + ConnectButton.w) &&
                                 (mouseY >= ConnectButton.y   && mouseY <= ConnectButton.y   + ConnectButton.h);

        if (inStart) {
            if (!game.tryStartPlayingWithBuiltDeck()) {
                game.setNextState(GameState::Playing);
            }
        } else if (inQuit) {
            game.setNextState(GameState::Quit);
        } else if (inBuildDeck) {
            game.setNextState(GameState::DeckBuilding);
        } else if (inOpenPacks) {
            game.setNextState(GameState::PackOpening);
        } else if (inConnect) {
            game.setNextState(GameState::Connecting);
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
    } else if (mouseX >= quitButton.x && mouseX <= quitButton.x + quitButton.w &&
               mouseY >= quitButton.y && mouseY <= quitButton.y + quitButton.h) {
        hoveredButton = 1;
    } else if (mouseX >= BuildDeckButton.x && mouseX <= BuildDeckButton.x + BuildDeckButton.w &&
               mouseY >= BuildDeckButton.y && mouseY <= BuildDeckButton.y + BuildDeckButton.h) {
        hoveredButton = 2;
    } else if (mouseX >= OpenPacksButton.x && mouseX <= OpenPacksButton.x + OpenPacksButton.w &&
               mouseY >= OpenPacksButton.y && mouseY <= OpenPacksButton.y + OpenPacksButton.h) {
        hoveredButton = 3;
    } else if (mouseX >= ConnectButton.x && mouseX <= ConnectButton.x + ConnectButton.w &&
               mouseY >= ConnectButton.y && mouseY <= ConnectButton.y + ConnectButton.h) {
        hoveredButton = 4;
    }
}

void Title::render(const Game& game) {
    SDL_Renderer*              renderer   = game.getRenderer();
    const RenderText::FontSet& titleFonts = game.getTitleFonts();
    const RenderText::FontSet& uiFonts    = game.getUIFonts();
    updateLayout(renderer);

    int screenW, screenH;
    SDL_GetRendererOutputSize(renderer, &screenW, &screenH);

    // ── animation timer ──────────────────────────────────────────────
    if (!animInitialized) {
        animStartTick   = SDL_GetTicks();
        animInitialized = true;
    }
    Uint32 now     = SDL_GetTicks();
    float  elapsed = (now - animStartTick) / 1000.0f;

    // ── background + vignette ────────────────────────────────────────
    SDL_SetRenderDrawColor(renderer, Theme::BG.r, Theme::BG.g, Theme::BG.b, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < 80; i++) {
        Uint8 alpha = (Uint8)(120 - i * 1.5f);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, alpha);
        SDL_Rect edge = {i, i, screenW - 2*i, screenH - 2*i};
        SDL_RenderDrawRect(renderer, &edge);
    }

    // ── animation values ─────────────────────────────────────────────

    // banner slides in from above
    float bannerT     = std::min(elapsed / 0.6f, 1.0f);
    float bannerEase  = 1.0f - (1.0f - bannerT) * (1.0f - bannerT);
    Uint8 bannerAlpha = (Uint8)(bannerEase * 255);
    int   bannerOffY  = (int)((1.0f - bannerEase) * -40);

    // buttons slide in staggered
    auto buttonSlide = [&](int index) -> std::pair<Uint8, int> {
        float delay = 0.3f + index * 0.15f;
        float t     = std::min(std::max((elapsed - delay) / 0.5f, 0.0f), 1.0f);
        float ease  = 1.0f - (1.0f - t) * (1.0f - t);
        return {(Uint8)(ease * 255), (int)((1.0f - ease) * -40)};
    };

    // ── draw banner ──────────────────────────────────────────────────
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

    RenderBanner::drawBanner(renderer, animBanner, "Ryan The Gathering",
                              titleFonts.large, animBannerFill, animGold,
                              animText, animGlow);

    // ── draw buttons ─────────────────────────────────────────────────
    auto drawBtn = [&](const SDL_Rect& rect, SDL_Color fill,
                        const std::string& text, int index) {
        auto [alpha, offY] = buttonSlide(index);
        SDL_Rect animRect  = {rect.x, rect.y + offY, rect.w, rect.h};

        // fade colors with slide-in alpha
        SDL_Color f = {fill.r,              fill.g,              fill.b,              alpha};
        SDL_Color b = {Theme::BTN_BORDER.r, Theme::BTN_BORDER.g, Theme::BTN_BORDER.b, alpha};
        SDL_Color t = {Theme::BTN_TEXT.r,   Theme::BTN_TEXT.g,   Theme::BTN_TEXT.b,   alpha};

        RenderButton::drawButton(renderer, animRect, text, uiFonts.large,
                                  f, b, t, hoveredButton == index);
    };

    drawBtn(startButton,     Theme::BTN_START,   "Start Game", 0);
    drawBtn(quitButton,      Theme::BTN_QUIT,    "Quit Game",  1);
    drawBtn(BuildDeckButton, Theme::BTN_BUILD,   "Build Deck", 2);
    drawBtn(OpenPacksButton, Theme::BTN_START,   "Open Packs", 3);
    drawBtn(ConnectButton,   Theme::BTN_CONNECT, "Connect",    4);
}