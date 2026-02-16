#include "states/Title.hpp"
#include "core/Game.hpp"
#include "render/RenderText.hpp"
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

    const int bannerW = titleBanner.w;
    const int bannerH = titleBanner.h;
    const int buttonW = startButton.w;
    const int buttonH = startButton.h;
    const int bannerGap = 24;
    const int buttonGap = 16;

    const int buttonsTotalH = (buttonH * 4) + (buttonGap * 3);
    const int totalH = bannerH + bannerGap + buttonsTotalH;
    int topY = (screenH - totalH) / 2;
    if (topY < 20) topY = 20;

    const int centerX = screenW / 2;

    titleBanner.x = centerX - (bannerW / 2);
    titleBanner.y = topY;

    startButton.x = centerX - (buttonW / 2);
    startButton.y = titleBanner.y + bannerH + bannerGap;

    quitButton.x = startButton.x;
    quitButton.y = startButton.y + buttonH + buttonGap;

    BuildDeckButton.x = startButton.x;
    BuildDeckButton.y = quitButton.y + buttonH + buttonGap;

    ConnectButton.x = startButton.x;
    ConnectButton.y = BuildDeckButton.y + buttonH + buttonGap;
}

void Title::handleEvents(Game& game, const SDL_Event& event) {
    updateLayout(game.getRenderer());

    if (event.type == SDL_QUIT)
    {
        game.setNextState(GameState::Quit);
    }

    if (event.type == SDL_MOUSEBUTTONDOWN &&
        event.button.button == SDL_BUTTON_LEFT)
    {
        int mouseX = event.button.x;
        int mouseY = event.button.y;

        const bool inStart = (mouseX >= startButton.x && mouseX <= (startButton.x + startButton.w)) &&
                             (mouseY >= startButton.y && mouseY <= (startButton.y + startButton.h));
        const bool inQuit = (mouseX >= quitButton.x && mouseX <= (quitButton.x + quitButton.w)) &&
                            (mouseY >= quitButton.y && mouseY <= (quitButton.y + quitButton.h));
        const bool inBuildDeck = (mouseX >= BuildDeckButton.x && mouseX <= (BuildDeckButton.x + BuildDeckButton.w)) &&
                                 (mouseY >= BuildDeckButton.y && mouseY <= (BuildDeckButton.y + BuildDeckButton.h));

        const bool inConnect = (mouseX >= ConnectButton.x && mouseX <= (ConnectButton.x + ConnectButton.w)) &&
                                 (mouseY >= ConnectButton.y && mouseY <= (ConnectButton.y + ConnectButton.h));

        if (inStart) {
            if (!game.tryStartPlayingWithBuiltDeck()) {
                game.setNextState(GameState::Playing);
            }
        } else if (inQuit) {
            game.setNextState(GameState::Quit);
        } else if (inBuildDeck) {
            game.setNextState(GameState::DeckBuilding);
        } else if (inConnect) {
            game.setNextState(GameState::Connecting);
        }
    }
}

void Title::update(Game& game) {
    int mouseX, mouseY;
    SDL_GetMouseState(&mouseX, &mouseY);

    auto inRect = [](int mx, int my, const SDL_Rect& r) {
        return mx >= r.x && mx <= r.x + r.w &&
               my >= r.y && my <= r.y + r.h;
    };

    if      (inRect(mouseX, mouseY, startButton))     hoveredButton = 0;
    else if (inRect(mouseX, mouseY, quitButton))      hoveredButton = 1;
    else if (inRect(mouseX, mouseY, BuildDeckButton)) hoveredButton = 2;
    else if (inRect(mouseX, mouseY, ConnectButton))   hoveredButton = 3;
    else  hoveredButton = -1;
}

void Title::render(Game& game) {
    SDL_Renderer* renderer = game.getRenderer();
    const RenderText::FontSet& titleFonts = game.getTitleFonts();
    const RenderText::FontSet& uiFonts    = game.getUIFonts();

    // initialize animation timer once
    if (!animInitialized) {
        animStartTick   = SDL_GetTicks();
        animInitialized = true;
    }
    Uint32 now     = SDL_GetTicks();
    float  elapsed = (now - animStartTick) / 1000.0f; // seconds since title loaded
    
    updateLayout(renderer);

    int screenW, screenH;
    SDL_GetRendererOutputSize(renderer, &screenW, &screenH);

    // ── helpers ──────────────────────────────────────────────────────────

    auto fillCircle = [&](int cx, int cy, int r) {
        for (int dy = -r; dy <= r; dy++) {
            int dx = (int)sqrt((double)(r * r - dy * dy));
            SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
        }
    };

    auto drawRounded = [&](const SDL_Rect& rect, SDL_Color fill, SDL_Color border) {
        const int r = 14;

        SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
        SDL_Rect body  = {rect.x + r,         rect.y,     rect.w - 2*r, rect.h      };
        SDL_Rect left  = {rect.x,              rect.y + r, r,            rect.h - 2*r};
        SDL_Rect right = {rect.x + rect.w - r, rect.y + r, r,            rect.h - 2*r};
        SDL_RenderFillRect(renderer, &body);
        SDL_RenderFillRect(renderer, &left);
        SDL_RenderFillRect(renderer, &right);
        fillCircle(rect.x + r,          rect.y + r,          r);
        fillCircle(rect.x + rect.w - r, rect.y + r,          r);
        fillCircle(rect.x + r,          rect.y + rect.h - r, r);
        fillCircle(rect.x + rect.w - r, rect.y + rect.h - r, r);

        // border
        SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
        SDL_RenderDrawLine(renderer, rect.x + r,        rect.y,          rect.x + rect.w - r, rect.y            );
        SDL_RenderDrawLine(renderer, rect.x + r,        rect.y + rect.h, rect.x + rect.w - r, rect.y + rect.h   );
        SDL_RenderDrawLine(renderer, rect.x,            rect.y + r,      rect.x,              rect.y + rect.h - r);
        SDL_RenderDrawLine(renderer, rect.x + rect.w,  rect.y + r,      rect.x + rect.w,     rect.y + rect.h - r);
    };

    auto drawShadow = [&](const SDL_Rect& rect) {
        const int r      = 14;
        const int offset = 5;
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 100);
        SDL_Rect s = {rect.x + offset, rect.y + offset, rect.w, rect.h};
        SDL_Rect sbody  = {s.x + r,        s.y,     s.w - 2*r, s.h        };
        SDL_Rect sleft  = {s.x,            s.y + r, r,         s.h - 2*r  };
        SDL_Rect sright = {s.x + s.w - r,  s.y + r, r,         s.h - 2*r  };
        SDL_RenderFillRect(renderer, &sbody);
        SDL_RenderFillRect(renderer, &sleft);
        SDL_RenderFillRect(renderer, &sright);
        fillCircle(s.x + r,       s.y + r,       r);
        fillCircle(s.x + s.w - r, s.y + r,       r);
        fillCircle(s.x + r,       s.y + s.h - r, r);
        fillCircle(s.x + s.w - r, s.y + s.h - r, r);
    };

    auto drawCentered = [&](TTF_Font* font, const std::string& text,
                             const SDL_Rect& rect, SDL_Color color) {
        if (!font) return;
        SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
        if (!surface) return;
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (texture) {
            SDL_Rect dst = {
                rect.x + (rect.w - surface->w) / 2,
                rect.y + (rect.h - surface->h) / 2,
                surface->w, surface->h
            };
            SDL_RenderCopy(renderer, texture, nullptr, &dst);
            SDL_DestroyTexture(texture);
        }
        SDL_FreeSurface(surface);
    };

    auto drawGlowText = [&](TTF_Font* font, const std::string& text,
                             const SDL_Rect& rect, SDL_Color glowColor, SDL_Color textColor) {
        if (!font) return;
        for (int offset = 4; offset >= 1; offset--) {
            Uint8 alpha = (Uint8)(15 + (4 - offset) * 15);
            SDL_Color glow = {glowColor.r, glowColor.g, glowColor.b, alpha};
            SDL_Surface* s = TTF_RenderUTF8_Blended(font, text.c_str(), glow);
            if (!s) continue;
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
            if (t) {
                int cx = rect.x + (rect.w - s->w) / 2;
                int cy = rect.y + (rect.h - s->h) / 2;
                for (int dx : {-offset, offset}) {
                    SDL_Rect dst = {cx + dx, cy, s->w, s->h};
                    SDL_RenderCopy(renderer, t, nullptr, &dst);
                }
                for (int dy : {-offset, offset}) {
                    SDL_Rect dst = {cx, cy + dy, s->w, s->h};
                    SDL_RenderCopy(renderer, t, nullptr, &dst);
                }
                SDL_DestroyTexture(t);
            }
            SDL_FreeSurface(s);
        }
        drawCentered(font, text, rect, textColor);
    };

    // ── colors ───────────────────────────────────────────────────────────

    SDL_Color ivory       = {220, 210, 185, 255};
    SDL_Color gold        = {240, 192, 64,  255};
    SDL_Color bannerFill  = {55,  20,  100, 255};  // rich violet purple
    SDL_Color startFill   = {35,  160, 130, 255};  // vibrant teal
    SDL_Color quitFill    = {185, 50,  70,  255};  // warm rose red
    SDL_Color buildFill   = {195, 155, 30,  255};  // bright antique gold
    SDL_Color connectFill = {75,  95,  140, 255};  // soft periwinkle blue
    SDL_Color titleText   = {220, 210, 185, 255};
    SDL_Color buttonText  = {240, 235, 220, 255};
    SDL_Color glowColor   = {120, 60,  220, 255};

    // ── background ───────────────────────────────────────────────────────

    SDL_SetRenderDrawColor(renderer,  18, 12, 35, 255);
    SDL_RenderClear(renderer);

    // vignette
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < 80; i++) {
        Uint8 alpha = (Uint8)(120 - i * 1.5f);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, alpha);
        SDL_Rect edge = {i, i, screenW - 2*i, screenH - 2*i};
        SDL_RenderDrawRect(renderer, &edge);
    }

    // ── animation values ─────────────────────────────────────────────────

    // title slides down and fades in over 0.6s
    float bannerT        = std::min(elapsed / 0.6f, 1.0f);
    float bannerEase     = 1.0f - (1.0f - bannerT) * (1.0f - bannerT);
    Uint8 bannerAlpha    = (Uint8)(bannerEase * 255);
    int   bannerOffY     = (int)((1.0f - bannerEase) * -40);

    auto buttonSlide = [&](int index) -> std::pair<Uint8, int> {
        float delay = 0.3f + index * 0.15f;
        float t     = std::min(std::max((elapsed - delay) / 0.5f, 0.0f), 1.0f);
        float ease  = 1.0f - (1.0f - t) * (1.0f - t);
        Uint8 alpha = (Uint8)(ease * 255);
        int   offY  = (int)((1.0f - ease) * -40);
        return {alpha, offY};
    };

    // hover pulse — gentle sine wave
    float pulse     = (float)(sin(now / 300.0f) * 0.5f + 0.5f);
    Uint8 pulseGlow = (Uint8)(120 + pulse * 80);

    // ── draw banner ──────────────────────────────────────────────────────

    SDL_Rect animBanner     = {titleBanner.x, titleBanner.y + bannerOffY, titleBanner.w, titleBanner.h};
    SDL_Color animGlow      = {glowColor.r,  glowColor.g,  glowColor.b,  bannerAlpha};
    SDL_Color animText      = {titleText.r,  titleText.g,  titleText.b,  bannerAlpha};
    SDL_Color animBannerFill= {bannerFill.r, bannerFill.g, bannerFill.b, bannerAlpha};
    SDL_Color animGold      = {gold.r,       gold.g,       gold.b,       bannerAlpha};

    drawShadow(animBanner);
    drawRounded(animBanner, animBannerFill, animGold);
    drawGlowText(titleFonts.large, "Speed Card Game", animBanner, animGlow, animText);

    // ── draw buttons ─────────────────────────────────────────────────────

    auto drawButton = [&](const SDL_Rect& rect, SDL_Color fill, SDL_Color border,
                        const std::string& text, int index) {
        auto [alpha, offY] = buttonSlide(index);

        SDL_Rect animRect = {rect.x, rect.y + offY, rect.w, rect.h};

        SDL_Color f = fill;
        SDL_Color b = border;

        if (hoveredButton == index) {
            // brighten fill on hover
            f.r = (Uint8)std::min(f.r + 50, 255);
            f.g = (Uint8)std::min(f.g + 50, 255);
            f.b = (Uint8)std::min(f.b + 50, 255);
            // pulsing glow border
            b = {255, 255, 255, pulseGlow};

            // glow halo — draw slightly larger rect behind button
            for (int i = 6; i >= 1; i--) {
                Uint8 haloAlpha = (Uint8)(pulse * 60 * (6 - i) / 6.0f);
                SDL_Color halo  = {f.r, f.g, f.b, haloAlpha};
                SDL_Rect haloRect = {animRect.x - i, animRect.y - i,
                                    animRect.w + i*2, animRect.h + i*2};
                drawRounded(haloRect, halo, {0, 0, 0, 0});
            }
        }

        f.a = alpha;
        b.a = (Uint8)(b.a * alpha / 255);
        SDL_Color t = {buttonText.r, buttonText.g, buttonText.b, alpha};

        drawShadow(animRect);
        drawRounded(animRect, f, b);
        drawCentered(uiFonts.large, text, animRect, t);
    };

    drawButton(startButton,     startFill,   ivory, "Start Game", 0);
    drawButton(quitButton,      quitFill,    ivory, "Quit Game",  1);
    drawButton(BuildDeckButton, buildFill,   ivory, "Build Deck", 2);
    drawButton(ConnectButton,   connectFill, ivory, "Connect",    3);
}