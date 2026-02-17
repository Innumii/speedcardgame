#ifndef THEME_HPP
#define THEME_HPP

#include <SDL2/SDL.h>

namespace Theme {

    // ── background ───────────────────────────────────────────────────
    constexpr SDL_Color BG                  = {18,  12,  35,  255};

    // ── banner ───────────────────────────────────────────────────────
    constexpr SDL_Color BANNER_FILL         = {55,  20,  100, 255};
    constexpr SDL_Color BANNER_BORDER       = {240, 192, 64,  255};
    constexpr SDL_Color BANNER_TEXT         = {220, 210, 185, 255};
    constexpr SDL_Color BANNER_GLOW         = {120, 60,  220, 255};

    // ── buttons ──────────────────────────────────────────────────────
    constexpr SDL_Color BTN_BORDER          = {220, 210, 185, 255};
    constexpr SDL_Color BTN_TEXT            = {240, 235, 220, 255};
    constexpr SDL_Color BTN_START           = {35,  160, 130, 255};
    constexpr SDL_Color BTN_QUIT            = {185, 50,  70,  255};
    constexpr SDL_Color BTN_BUILD           = {195, 155, 30,  255};
    constexpr SDL_Color BTN_CONNECT         = {75,  95,  140, 255};
    constexpr SDL_Color BTN_PRIMARY         = {70,  120, 200, 255};
    constexpr SDL_Color BTN_SECONDARY       = {70,  70,  70,  255};

    // ── text ─────────────────────────────────────────────────────────
    constexpr SDL_Color TEXT_PRIMARY        = {245, 245, 245, 255};
    constexpr SDL_Color TEXT_MUTED          = {190, 190, 190, 255};
    constexpr SDL_Color TEXT_IVORY          = {220, 210, 185, 255};

    // ── input fields ─────────────────────────────────────────────────
    constexpr SDL_Color INPUT_FILL          = {25,  22,  45,  255};
    constexpr SDL_Color INPUT_ACTIVE        = {40,  35,  70,  255};
    constexpr SDL_Color INPUT_BORDER        = {100, 100, 120, 255};
    constexpr SDL_Color INPUT_BORDER_ACTIVE = {120, 60,  220, 255};
    constexpr SDL_Color INPUT_BORDER_IDLE   = {100, 100, 120, 255};

    // ── panel ────────────────────────────────────────────────────────
    constexpr SDL_Color PANEL_FILL          = {30,  25,  50,  230};
    constexpr SDL_Color PANEL_BORDER        = {240, 192, 64,  100};

    // ── feedback ─────────────────────────────────────────────────────
    constexpr SDL_Color ERROR_RED           = {220, 90,  90,  255};
    constexpr SDL_Color SUCCESS_GREEN       = {80,  200, 120, 255};

    // ── sizing ───────────────────────────────────────────────────────
    constexpr int BTN_W                     = 260;
    constexpr int BTN_H                     = 65;
    constexpr int BTN_RADIUS                = 14;
    constexpr int INPUT_RADIUS              = 8;
    constexpr int PANEL_RADIUS              = 16;
    constexpr int BANNER_W                  = 640;
    constexpr int BANNER_H                  = 100;
}

#endif