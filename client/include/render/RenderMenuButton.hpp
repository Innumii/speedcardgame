#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// RenderMenuButton
//
// Draws a flat, minimal menu item:
//
//   ▐  Button Text
//
// A solid white vertical bar sits to the left of the label.
// No background rectangle, no rounded corners.
// Intended for use in the Title screen.
// ─────────────────────────────────────────────────────────────────────────────
class RenderMenuButton {
public:
    // Width of the vertical accent bar (pixels, pre-scale — pass already-scaled values)
    static constexpr int BAR_WIDTH = 4;
    // Gap between the bar and the text
    static constexpr int BAR_GAP   = 14;

    // Draw the button at (x, y).
    // alpha controls overall opacity (used for slide-in animation).
    static void draw(SDL_Renderer*      renderer,
                     int                x,
                     int                y,
                     const std::string& text,
                     TTF_Font*          font,
                     bool               hovered = false,
                     Uint8              alpha   = 255);

    // Returns the full bounding SDL_Rect (bar + gap + text) anchored at (x, y).
    // Useful for hit-testing.
    static SDL_Rect measure(TTF_Font*          font,
                            const std::string& text,
                            int                x,
                            int                y);
};