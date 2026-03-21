#include "states/Register.hpp"
#include "core/Game.hpp"
#include "render/RenderButton.hpp"
#include "render/RenderBackdrop.hpp"
#include "render/RenderBanner.hpp"
#include "render/RenderText.hpp"
#include "render/Theme.hpp"
#include "utils/HttpUtil.hpp"
#include "utils/JsonUtil.hpp"
#include "utils/EnvUtil.hpp"
#include "utils/RenderUtil.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <cmath>

// ── network helpers ───────────────────────────────────────────────────────────

namespace {
    constexpr std::size_t kMaxEmailLen    = 64;
    constexpr std::size_t kMaxPasswordLen = 32;

    std::string deriveNameFromEmail(const std::string& email) {
        std::size_t atPos = email.find('@');
        if (atPos == std::string::npos || atPos == 0) return "Player";
        std::string name = email.substr(0, atPos);
        return name.empty() ? "Player" : name;
    }

    bool isLikelyEmail(const std::string& email) {
        std::size_t atPos = email.find('@');
        if (atPos == std::string::npos || atPos == 0 || atPos + 1 >= email.size()) return false;
        std::size_t dotPos = email.find('.', atPos + 1);
        return dotPos != std::string::npos && dotPos + 1 < email.size();
    }

    bool registerUser(const std::string& email, const std::string& password, std::string& error) {
        if (email.empty() || password.empty()) {
            error = "email and password required";
            return false;
        }

        const std::string host = EnvUtil::getAuthServiceHost();
        const int         port = EnvUtil::getAuthServicePort();
        const std::string name = deriveNameFromEmail(email);

        std::ostringstream payload;
        payload << "{\"name\":\""       << JsonUtil::escapeJsonString(name)
                << "\",\"email\":\""    << JsonUtil::escapeJsonString(email)
                << "\",\"password\":\"" << JsonUtil::escapeJsonString(password) << "\"}";

        int statusCode = -1;
        std::string responseBody;
        if (!HttpUtil::sendHttp(host, port, "POST", "/auth/register",
                                payload.str(), statusCode, responseBody)) {
            error = "auth service unreachable";
            return false;
        }

        if (statusCode != 201) {
            if (!JsonUtil::readJsonStringField(responseBody, "error", error))
                error = "registration failed";
            return false;
        }

        return true;
    }

} // ── end anonymous namespace ──────────────────────────────────────────────────

// ── lifecycle ─────────────────────────────────────────────────────────────────

Register::Register()  = default;
Register::~Register() = default;

void Register::enter(Game& game) {
    setActiveField(Field::Email);
    statusMessage.clear();
    SDL_StartTextInput();
}

void Register::exit(Game& game) {
    SDL_StopTextInput();
    createPressed = false;
    backPressed   = false;
    createHover   = false;
    backHover     = false;
}

// ── helpers ───────────────────────────────────────────────────────────────────

void Register::setActiveField(Field field) {
    activeField = field;
    activeField == Field::None ? SDL_StopTextInput() : SDL_StartTextInput();
}

void Register::updateLayout(SDL_Renderer* renderer) {
    int screenW = 800, screenH = 600;
    if (renderer) SDL_GetRendererOutputSize(renderer, &screenW, &screenH);

    const int panelW = std::min(560, static_cast<int>(screenW * 0.75f));
    const int panelH = 500;
    panelRect = {(screenW - panelW) / 2, (screenH - panelH) / 2 + 60, panelW, panelH};

    const int inputW  = panelW - 80;
    const int inputH  = 52;
    const int inputX  = panelRect.x + 40;
    int       cursorY = panelRect.y + 110;

    emailRect    = {inputX, cursorY, inputW, inputH};
    cursorY += inputH + 40;

    passwordRect = {inputX, cursorY, inputW, inputH};
    cursorY += inputH + 40;

    confirmRect  = {inputX, cursorY, inputW, inputH};
    cursorY += inputH + 36;

    const int buttonW = (inputW - 16) / 2;
    const int buttonH = 52;
    createButtonRect = {inputX,               cursorY, buttonW, buttonH};
    backButtonRect   = {inputX + buttonW + 16, cursorY, buttonW, buttonH};
}

// ── events ────────────────────────────────────────────────────────────────────

void Register::handleEvents(Game& game, const SDL_Event& event) {
    updateLayout(game.getRenderer());

    if (event.type == SDL_QUIT) {
        game.setNextState(GameState::Quit);
        return;
    }

    if (event.type == SDL_MOUSEMOTION) {
        createHover = RenderUtil::pointInRect(createButtonRect, event.motion.x, event.motion.y);
        backHover   = RenderUtil::pointInRect(backButtonRect,   event.motion.x, event.motion.y);
    }

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        const int mx = event.button.x, my = event.button.y;
        if      (RenderUtil::pointInRect(emailRect,    mx, my)) setActiveField(Field::Email);
        else if (RenderUtil::pointInRect(passwordRect, mx, my)) setActiveField(Field::Password);
        else if (RenderUtil::pointInRect(confirmRect,  mx, my)) setActiveField(Field::ConfirmPassword);
        else                                        setActiveField(Field::None);

        if      (RenderUtil::pointInRect(createButtonRect, mx, my)) createPressed = true;
        else if (RenderUtil::pointInRect(backButtonRect,   mx, my)) backPressed   = true;
    }

    if (event.type == SDL_TEXTINPUT) {
        statusMessage.clear();
        if      (activeField == Field::Email           && email.size()           < kMaxEmailLen)
            email.append(event.text.text);
        else if (activeField == Field::Password        && password.size()        < kMaxPasswordLen)
            password.append(event.text.text);
        else if (activeField == Field::ConfirmPassword && confirmPassword.size() < kMaxPasswordLen)
            confirmPassword.append(event.text.text);
    }

    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
            case SDLK_TAB:
                if      (activeField == Field::Email)    setActiveField(Field::Password);
                else if (activeField == Field::Password) setActiveField(Field::ConfirmPassword);
                else                                     setActiveField(Field::Email);
                break;
            case SDLK_BACKSPACE:
                statusMessage.clear();
                if      (activeField == Field::Email           && !email.empty())           email.pop_back();
                else if (activeField == Field::Password        && !password.empty())        password.pop_back();
                else if (activeField == Field::ConfirmPassword && !confirmPassword.empty()) confirmPassword.pop_back();
                break;
            case SDLK_RETURN: case SDLK_KP_ENTER:
                createPressed = true; break;
            case SDLK_ESCAPE:
                backPressed = true; break;
            default: break;
        }
    }
}

// ── update ────────────────────────────────────────────────────────────────────

void Register::update(Game& game) {
    if (createPressed) {
        createPressed = false;
        statusMessage.clear();

        if (email.empty())               { statusMessage = "Email is required.";                       return; }
        if (!isLikelyEmail(email))       { statusMessage = "Enter a valid email address.";             return; }
        if (password.empty())            { statusMessage = "Password is required.";                    return; }
        if (confirmPassword.empty())     { statusMessage = "Re-enter your password.";                  return; }
        if (password.size() < 8)         { statusMessage = "Password must be at least 8 characters."; return; }
        if (password != confirmPassword) { statusMessage = "Passwords do not match.";                  return; }

        std::string error;
        if (!registerUser(email, password, error)) {
            statusMessage = error;
            std::cerr << "Registration failed: " << error << '\n';
            return;
        }

        email.clear();
        password.clear();
        confirmPassword.clear();
        statusMessage.clear();
        game.setNextState(GameState::Login);
        return;
    }

    if (backPressed) {
        backPressed = false;
        game.setNextState(GameState::Login);
    }
}

// ── render ────────────────────────────────────────────────────────────────────

void Register::render(const Game& game) {
    SDL_Renderer*              renderer   = game.getRenderer();
    const RenderText::FontSet& titleFonts = game.getTitleFonts();
    const RenderText::FontSet& uiFonts    = game.getUIFonts();
    updateLayout(renderer);

    int screenW, screenH;
    SDL_GetRendererOutputSize(renderer, &screenW, &screenH);

    // ── background + vignette ────────────────────────────────────────
    RenderBackdrop::drawBackgroundWithVignette(
        renderer,
        screenW,
        screenH,
        Theme::BG,
        SDL_Color{0, 0, 0, 255},
        80,
        1.5f,
        120
    );

    // ── banner ───────────────────────────────────────────────────────
    SDL_Rect bannerRect = {screenW / 2 - Theme::BANNER_W / 2, 30,
                           Theme::BANNER_W, Theme::BANNER_H};
    RenderBanner::drawBanner(renderer, bannerRect, "Mana Kaisen",
                              titleFonts.large,
                              Theme::BANNER_FILL,  Theme::BANNER_BORDER,
                              Theme::BANNER_TEXT,  Theme::BANNER_GLOW);

    // ── panel ────────────────────────────────────────────────────────
    RenderUtil::drawRoundedShadow(renderer, panelRect, Theme::PANEL_RADIUS, Theme::Effects::SHADOW_OFFSET, Theme::Effects::SHADOW_COLOR);
    RenderUtil::drawRoundedRect(renderer, panelRect, Theme::PANEL_RADIUS, Theme::PANEL_FILL, Theme::PANEL_BORDER);

    // ── panel title ──────────────────────────────────────────────────
    if (titleFonts.medium) {
        RenderText::drawText(renderer, "Create Account", titleFonts.medium,
                             Theme::TEXT_PRIMARY, panelRect.x + 26, panelRect.y + 22);
    }

    // ── field labels ─────────────────────────────────────────────────
    if (uiFonts.large) {
        RenderText::drawText(renderer, "Email",            uiFonts.large,
                             Theme::TEXT_MUTED, emailRect.x,    emailRect.y    - 28);
        RenderText::drawText(renderer, "Password",         uiFonts.large,
                             Theme::TEXT_MUTED, passwordRect.x, passwordRect.y - 28);
        RenderText::drawText(renderer, "Confirm Password", uiFonts.large,
                             Theme::TEXT_MUTED, confirmRect.x,  confirmRect.y  - 28);
    }

    // ── input fields ─────────────────────────────────────────────────
    SDL_Color emailFill     = activeField == Field::Email           ? Theme::INPUT_ACTIVE : Theme::INPUT_FILL;
    SDL_Color passFill      = activeField == Field::Password        ? Theme::INPUT_ACTIVE : Theme::INPUT_FILL;
    SDL_Color confirmFill   = activeField == Field::ConfirmPassword ? Theme::INPUT_ACTIVE : Theme::INPUT_FILL;

    SDL_Color emailBorder   = activeField == Field::Email           ? Theme::INPUT_BORDER_ACTIVE : Theme::INPUT_BORDER_IDLE;
    SDL_Color passBorder    = activeField == Field::Password        ? Theme::INPUT_BORDER_ACTIVE : Theme::INPUT_BORDER_IDLE;
    SDL_Color confirmBorder = activeField == Field::ConfirmPassword ? Theme::INPUT_BORDER_ACTIVE : Theme::INPUT_BORDER_IDLE;

    SDL_SetRenderDrawColor(renderer, emailFill.r,     emailFill.g,     emailFill.b,     emailFill.a);
    SDL_RenderFillRect(renderer, &emailRect);
    SDL_SetRenderDrawColor(renderer, emailBorder.r,   emailBorder.g,   emailBorder.b,   emailBorder.a);
    SDL_RenderDrawRect(renderer, &emailRect);

    SDL_SetRenderDrawColor(renderer, passFill.r,      passFill.g,      passFill.b,      passFill.a);
    SDL_RenderFillRect(renderer, &passwordRect);
    SDL_SetRenderDrawColor(renderer, passBorder.r,    passBorder.g,    passBorder.b,    passBorder.a);
    SDL_RenderDrawRect(renderer, &passwordRect);

    SDL_SetRenderDrawColor(renderer, confirmFill.r,   confirmFill.g,   confirmFill.b,   confirmFill.a);
    SDL_RenderFillRect(renderer, &confirmRect);
    SDL_SetRenderDrawColor(renderer, confirmBorder.r, confirmBorder.g, confirmBorder.b, confirmBorder.a);
    SDL_RenderDrawRect(renderer, &confirmRect);

    // ── input text ───────────────────────────────────────────────────
    if (uiFonts.large) {
        const int textPad = 10;
        RenderText::drawText(renderer, email, uiFonts.large,
                             Theme::TEXT_PRIMARY,
                             emailRect.x + textPad,
                             emailRect.y + (emailRect.h - 20) / 2);
        std::string passMask(password.size(), '*');
        RenderText::drawText(renderer, passMask, uiFonts.large,
                             Theme::TEXT_PRIMARY,
                             passwordRect.x + textPad,
                             passwordRect.y + (passwordRect.h - 20) / 2);
        std::string confirmMask(confirmPassword.size(), '*');
        RenderText::drawText(renderer, confirmMask, uiFonts.large,
                             Theme::TEXT_PRIMARY,
                             confirmRect.x + textPad,
                             confirmRect.y + (confirmRect.h - 20) / 2);
    }

    // ── validation message ───────────────────────────────────────────
    std::string warningMessage = statusMessage;
    if (warningMessage.empty() && !confirmPassword.empty() && password != confirmPassword)
        warningMessage = "Passwords do not match.";

    if (uiFonts.small && !warningMessage.empty()) {
        RenderText::drawText(renderer, warningMessage, uiFonts.small,
                             Theme::ERROR_RED,
                             confirmRect.x, confirmRect.y + confirmRect.h + 6);
    }

    // ── buttons ──────────────────────────────────────────────────────
    RenderButton::drawButton(renderer, createButtonRect, "Create",
                              uiFonts.large,
                              Theme::BTN_PRIMARY,   Theme::BTN_BORDER,
                              Theme::BTN_TEXT,       createHover, false);

    RenderButton::drawButton(renderer, backButtonRect,   "Back",
                              uiFonts.large,
                              Theme::BTN_SECONDARY, Theme::BTN_BORDER,
                              Theme::BTN_TEXT,       backHover, false);
}