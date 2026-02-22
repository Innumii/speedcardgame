#include "states/Register.hpp"
#include "core/Game.hpp"
#include "core/NetworkClient.hpp"
#include "render/RenderButton.hpp"
#include "render/RenderBanner.hpp"
#include "render/RenderText.hpp"
#include "render/Theme.hpp"
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

    std::string getEnvOrDefault(const char* key, const char* fallback) {
        const char* value = std::getenv(key);
        return value ? std::string(value) : std::string(fallback);
    }

    int getEnvIntOrDefault(const char* key, int fallback) {
        const char* value = std::getenv(key);
        if (!value) return fallback;
        try { return std::stoi(value); } catch (...) { return fallback; }
    }

    std::string escapeJsonString(const std::string& value) {
        std::string out;
        out.reserve(value.size());
        for (char c : value) {
            switch (c) {
                case '\\': out += "\\\\"; break;
                case '"':  out += "\\\""; break;
                case '\n': out += "\\n";  break;
                case '\r': out += "\\r";  break;
                case '\t': out += "\\t";  break;
                default:   out += c;      break;
            }
        }
        return out;
    }

    bool readJsonStringField(const std::string& json, const std::string& key, std::string& out) {
        const std::string needle = "\"" + key + "\"";
        std::size_t pos = json.find(needle);
        if (pos == std::string::npos) return false;
        pos = json.find(':', pos + needle.size());
        if (pos == std::string::npos) return false;
        pos = json.find('"', pos);
        if (pos == std::string::npos) return false;
        std::size_t end = pos + 1;
        while (end < json.size()) {
            if (json[end] == '"' && json[end - 1] != '\\') break;
            ++end;
        }
        if (end >= json.size()) return false;
        out = json.substr(pos + 1, end - pos - 1);
        return true;
    }

    bool sendHttpRequest(const std::string& host, int port, const std::string& method,
                         const std::string& path, const std::string& body,
                         int& statusCode, std::string& responseBody) {
        statusCode = -1;
        NetworkClient client(NetworkClient::SocketMode::Blocking); // blocking!
        if (!client.connectTo(host, port)) return false;

        std::ostringstream request;
        request << method << " " << path << " HTTP/1.1\r\n"
                << "Host: " << host << "\r\n"
                << "Connection: close\r\n";
        if (method == "POST" || method == "PUT" || method == "PATCH") {
            request << "Content-Type: application/json\r\n"
                    << "Content-Length: " << body.size() << "\r\n";
        }
        request << "\r\n" << body;

        const std::string requestText = request.str();
        if (!client.send(requestText.data(), requestText.size())) {
            client.disconnect();
            return false;
        }

        std::string response;
        char buffer[4096];
        while (true) {
            int received = client.receive(buffer, sizeof(buffer));
            if (received <= 0) break;
            response.append(buffer, static_cast<std::size_t>(received));
        }
        client.disconnect();

        const std::size_t headerEnd = response.find("\r\n\r\n");
        if (headerEnd == std::string::npos) return false;

        std::istringstream headerStream(response.substr(0, headerEnd));
        std::string httpVersion;
        headerStream >> httpVersion >> statusCode;

        responseBody = response.substr(headerEnd + 4);
        return true;
    }

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

        const std::string host = getEnvOrDefault("AUTH_SERVICE_HOST", "127.0.0.1");
        const int         port = getEnvIntOrDefault("AUTH_SERVICE_PORT", 8081);
        const std::string name = deriveNameFromEmail(email);

        std::ostringstream payload;
        payload << "{\"name\":\""       << escapeJsonString(name)
                << "\",\"email\":\""    << escapeJsonString(email)
                << "\",\"password\":\"" << escapeJsonString(password) << "\"}";

        int statusCode = -1;
        std::string responseBody;
        if (!sendHttpRequest(host, port, "POST", "/register",
                             payload.str(), statusCode, responseBody)) {
            error = "auth service unreachable";
            return false;
        }

        if (statusCode != 201) {
            if (!readJsonStringField(responseBody, "error", error))
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

bool Register::pointInRect(const SDL_Rect& rect, int x, int y) const {
    return x >= rect.x && x <= rect.x + rect.w &&
           y >= rect.y && y <= rect.y + rect.h;
}

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
        createHover = pointInRect(createButtonRect, event.motion.x, event.motion.y);
        backHover   = pointInRect(backButtonRect,   event.motion.x, event.motion.y);
    }

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        const int mx = event.button.x, my = event.button.y;
        if      (pointInRect(emailRect,    mx, my)) setActiveField(Field::Email);
        else if (pointInRect(passwordRect, mx, my)) setActiveField(Field::Password);
        else if (pointInRect(confirmRect,  mx, my)) setActiveField(Field::ConfirmPassword);
        else                                        setActiveField(Field::None);

        if      (pointInRect(createButtonRect, mx, my)) createPressed = true;
        else if (pointInRect(backButtonRect,   mx, my)) backPressed   = true;
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

void Register::render(Game& game) {
    SDL_Renderer*              renderer   = game.getRenderer();
    const RenderText::FontSet& titleFonts = game.getTitleFonts();
    const RenderText::FontSet& uiFonts    = game.getUIFonts();
    updateLayout(renderer);

    int screenW, screenH;
    SDL_GetRendererOutputSize(renderer, &screenW, &screenH);

    // ── rounded rect helper ──────────────────────────────────────────
    auto fillCircle = [&](int cx, int cy, int r) {
        for (int dy = -r; dy <= r; dy++) {
            int dx = (int)sqrt((double)(r*r - dy*dy));
            SDL_RenderDrawLine(renderer, cx-dx, cy+dy, cx+dx, cy+dy);
        }
    };

    auto fillRounded = [&](const SDL_Rect& r, SDL_Color fill, SDL_Color border) {
        const int rad = Theme::PANEL_RADIUS;
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
        SDL_Rect body  = {r.x + rad,        r.y,       r.w - 2*rad, r.h        };
        SDL_Rect left  = {r.x,              r.y + rad, rad,         r.h - 2*rad};
        SDL_Rect right = {r.x + r.w - rad,  r.y + rad, rad,         r.h - 2*rad};
        SDL_RenderFillRect(renderer, &body);
        SDL_RenderFillRect(renderer, &left);
        SDL_RenderFillRect(renderer, &right);
        fillCircle(r.x + rad,        r.y + rad,       rad);
        fillCircle(r.x + r.w - rad,  r.y + rad,       rad);
        fillCircle(r.x + rad,        r.y + r.h - rad, rad);
        fillCircle(r.x + r.w - rad,  r.y + r.h - rad, rad);
        SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
        SDL_RenderDrawLine(renderer, r.x + rad,    r.y,         r.x + r.w - rad, r.y            );
        SDL_RenderDrawLine(renderer, r.x + rad,    r.y + r.h,   r.x + r.w - rad, r.y + r.h      );
        SDL_RenderDrawLine(renderer, r.x,          r.y + rad,   r.x,             r.y + r.h - rad);
        SDL_RenderDrawLine(renderer, r.x + r.w,    r.y + rad,   r.x + r.w,       r.y + r.h - rad);
    };

    // ── background + vignette ────────────────────────────────────────
    SDL_SetRenderDrawColor(renderer, Theme::BG.r, Theme::BG.g, Theme::BG.b, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < 80; i++) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, (Uint8)(120 - i * 1.5f));
        SDL_Rect edge = {i, i, screenW - 2*i, screenH - 2*i};
        SDL_RenderDrawRect(renderer, &edge);
    }

    // ── banner ───────────────────────────────────────────────────────
    SDL_Rect bannerRect = {screenW / 2 - Theme::BANNER_W / 2, 30,
                           Theme::BANNER_W, Theme::BANNER_H};
    RenderBanner::drawBanner(renderer, bannerRect, "Speed Card Game",
                              titleFonts.large,
                              Theme::BANNER_FILL,  Theme::BANNER_BORDER,
                              Theme::BANNER_TEXT,  Theme::BANNER_GLOW);

    // ── panel ────────────────────────────────────────────────────────
    fillRounded(panelRect, Theme::PANEL_FILL, Theme::PANEL_BORDER);

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