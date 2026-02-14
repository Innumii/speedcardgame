#include "states/Register.hpp"

#include "core/Game.hpp"
#include "core/NetworkClient.hpp"
#include "render/RenderButton.hpp"
#include "render/RenderText.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <sstream>

namespace {
    constexpr std::size_t kMaxEmailLen = 64;
    constexpr std::size_t kMaxPasswordLen = 32;

    std::string getEnvOrDefault(const char* key, const char* fallback) {
        const char* value = std::getenv(key);
        return value ? std::string(value) : std::string(fallback);
    }

    int getEnvIntOrDefault(const char* key, int fallback) {
        const char* value = std::getenv(key);
        if (!value) return fallback;
        try {
            return std::stoi(value);
        } catch (...) {
            return fallback;
        }
    }

    std::string escapeJsonString(const std::string& value) {
        std::string out;
        out.reserve(value.size());
        for (char c : value) {
            switch (c) {
                case '\\': out += "\\\\"; break;
                case '"': out += "\\\""; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default: out += c; break;
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

    bool sendHttpRequest(const std::string& host, int port, const std::string& method, const std::string& path,
                         const std::string& body, int& statusCode, std::string& responseBody) {
        statusCode = -1;
        NetworkClient client;
        if (!client.connectTo(host, port)) {
            return false;
        }

        std::ostringstream request;
        request << method << " " << path << " HTTP/1.1\r\n";
        request << "Host: " << host << "\r\n";
        request << "Connection: close\r\n";
        if (method == "POST" || method == "PUT" || method == "PATCH") {
            request << "Content-Type: application/json\r\n";
            request << "Content-Length: " << body.size() << "\r\n";
        }
        request << "\r\n";
        request << body;

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

        const std::string header = response.substr(0, headerEnd);
        std::istringstream headerStream(header);
        std::string httpVersion;
        headerStream >> httpVersion >> statusCode;

        responseBody = response.substr(headerEnd + 4);
        return true;
    }

    std::string deriveNameFromEmail(const std::string& email) {
        std::size_t atPos = email.find('@');
        if (atPos == std::string::npos || atPos == 0) {
            return "Player";
        }
        std::string name = email.substr(0, atPos);
        if (name.empty()) {
            return "Player";
        }
        return name;
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
        const int port = getEnvIntOrDefault("AUTH_SERVICE_PORT", 8081);
        const std::string path = "/register";
        const std::string name = deriveNameFromEmail(email);

        std::ostringstream payload;
        payload << "{\"name\":\"" << escapeJsonString(name)
                << "\",\"email\":\"" << escapeJsonString(email)
                << "\",\"password\":\"" << escapeJsonString(password) << "\"}";

        int statusCode = -1;
        std::string responseBody;
        if (!sendHttpRequest(host, port, "POST", path, payload.str(), statusCode, responseBody)) {
            error = "auth service unreachable";
            return false;
        }

        if (statusCode != 201) {
            std::string responseError;
            if (readJsonStringField(responseBody, "error", responseError)) {
                error = responseError;
            } else {
                error = "registration failed";
            }
            return false;
        }

        return true;
    }
}

Register::Register() = default;

Register::~Register() {
    RenderText::closeFonts(fonts);
}

void Register::enter(Game& game) {
    ensureFonts();
    setActiveField(Field::Email);
    statusMessage.clear();
    SDL_StartTextInput();
}

void Register::exit(Game& game) {
    SDL_StopTextInput();
    createPressed = false;
    backPressed = false;
    createHover = false;
    backHover = false;
}

void Register::ensureFonts() {
    if (fontsReady) return;
    if (!RenderText::ensureTtfReady()) return;

    fonts = RenderText::loadFonts("assets/font.TTF", 16, 12, 28);
    if (!fonts.small || !fonts.large) {
        RenderText::closeFonts(fonts);
        fontsReady = false;
        return;
    }

    fontsReady = true;
}

bool Register::pointInRect(const SDL_Rect& rect, int x, int y) const {
    return x >= rect.x && x <= rect.x + rect.w && y >= rect.y && y <= rect.y + rect.h;
}

void Register::setActiveField(Field field) {
    activeField = field;
    if (activeField == Field::None) {
        SDL_StopTextInput();
    } else {
        SDL_StartTextInput();
    }
}

void Register::updateLayout(SDL_Renderer* renderer) {
    int screenW = 800;
    int screenH = 600;
    if (renderer) {
        SDL_GetRendererOutputSize(renderer, &screenW, &screenH);
    }

    const int panelW = std::min(560, static_cast<int>(screenW * 0.8f));
    const int panelH = 440;
    panelRect = SDL_Rect{
        (screenW - panelW) / 2,
        (screenH - panelH) / 2,
        panelW,
        panelH
    };

    const int inputW = panelW - 120;
    const int inputH = 44;
    const int inputX = panelRect.x + 60;
    int cursorY = panelRect.y + 78;

    emailRect = SDL_Rect{inputX, cursorY, inputW, inputH};
    cursorY += inputH + 32;

    passwordRect = SDL_Rect{inputX, cursorY, inputW, inputH};
    cursorY += inputH + 32;

    confirmRect = SDL_Rect{inputX, cursorY, inputW, inputH};
    cursorY += inputH + 46;

    const int buttonW = (inputW - 20) / 2;
    const int buttonH = 46;
    createButtonRect = SDL_Rect{inputX, cursorY, buttonW, buttonH};
    backButtonRect = SDL_Rect{inputX + buttonW + 20, cursorY, buttonW, buttonH};
}

void Register::handleEvents(Game& game, const SDL_Event& event) {
    updateLayout(game.getRenderer());

    if (event.type == SDL_QUIT) {
        game.setNextState(GameState::Quit);
        return;
    }

    if (event.type == SDL_MOUSEMOTION) {
        const int mouseX = event.motion.x;
        const int mouseY = event.motion.y;
        createHover = pointInRect(createButtonRect, mouseX, mouseY);
        backHover = pointInRect(backButtonRect, mouseX, mouseY);
    }

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        const int mouseX = event.button.x;
        const int mouseY = event.button.y;

        if (pointInRect(emailRect, mouseX, mouseY)) {
            setActiveField(Field::Email);
        } else if (pointInRect(passwordRect, mouseX, mouseY)) {
            setActiveField(Field::Password);
        } else if (pointInRect(confirmRect, mouseX, mouseY)) {
            setActiveField(Field::ConfirmPassword);
        } else {
            setActiveField(Field::None);
        }

        if (pointInRect(createButtonRect, mouseX, mouseY)) {
            createPressed = true;
        } else if (pointInRect(backButtonRect, mouseX, mouseY)) {
            backPressed = true;
        }
    }

    if (event.type == SDL_TEXTINPUT) {
        statusMessage.clear();
        if (activeField == Field::Email && email.size() < kMaxEmailLen) {
            email.append(event.text.text);
        } else if (activeField == Field::Password && password.size() < kMaxPasswordLen) {
            password.append(event.text.text);
        } else if (activeField == Field::ConfirmPassword && confirmPassword.size() < kMaxPasswordLen) {
            confirmPassword.append(event.text.text);
        }
    }

    if (event.type == SDL_KEYDOWN) {
        if (event.key.keysym.sym == SDLK_TAB) {
            if (activeField == Field::Email) {
                setActiveField(Field::Password);
            } else if (activeField == Field::Password) {
                setActiveField(Field::ConfirmPassword);
            } else {
                setActiveField(Field::Email);
            }
        } else if (event.key.keysym.sym == SDLK_BACKSPACE) {
            statusMessage.clear();
            if (activeField == Field::Email && !email.empty()) {
                email.pop_back();
            } else if (activeField == Field::Password && !password.empty()) {
                password.pop_back();
            } else if (activeField == Field::ConfirmPassword && !confirmPassword.empty()) {
                confirmPassword.pop_back();
            }
        } else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER) {
            createPressed = true;
        } else if (event.key.keysym.sym == SDLK_ESCAPE) {
            backPressed = true;
        }
    }
}

void Register::update(Game& game) {
    if (createPressed) {
        createPressed = false;
        statusMessage.clear();

        if (email.empty()) {
            statusMessage = "Email is required.";
            return;
        }
        if (!isLikelyEmail(email)) {
            statusMessage = "Enter a valid email address.";
            return;
        }
        if (password.empty()) {
            statusMessage = "Password is required.";
            return;
        }
        if (confirmPassword.empty()) {
            statusMessage = "Re-enter your password.";
            return;
        }
        if (password.size() < 8) {
            statusMessage = "Password must be at least 8 characters.";
            return;
        }
        if (password != confirmPassword) {
            statusMessage = "Passwords do not match.";
            return;
        }

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

void Register::render(Game& game) {
    SDL_Renderer* renderer = game.getRenderer();
    updateLayout(renderer);
    ensureFonts();

    SDL_SetRenderDrawColor(renderer, 16, 16, 18, 255);
    SDL_RenderClear(renderer);

    SDL_Color panelFill{34, 34, 36, 240};
    SDL_Color panelOutline{120, 120, 125, 220};
    SDL_Color white{245, 245, 245, 255};
    SDL_Color muted{175, 175, 180, 255};
    SDL_Color fieldFill{24, 24, 26, 255};
    SDL_Color fieldActive{40, 40, 44, 255};
    SDL_Color fieldOutline{110, 110, 115, 255};
    SDL_Color errorRed{220, 90, 90, 255};

    SDL_SetRenderDrawColor(renderer, panelFill.r, panelFill.g, panelFill.b, panelFill.a);
    SDL_RenderFillRect(renderer, &panelRect);
    SDL_SetRenderDrawColor(renderer, panelOutline.r, panelOutline.g, panelOutline.b, panelOutline.a);
    SDL_RenderDrawRect(renderer, &panelRect);

    if (fonts.large) {
        RenderText::drawText(renderer, "Create Account", fonts.large, white, panelRect.x + 26, panelRect.y + 20);
    }

    if (fonts.small) {
        RenderText::drawText(renderer, "Email", fonts.small, muted, emailRect.x, emailRect.y - 22);
        RenderText::drawText(renderer, "Password", fonts.small, muted, passwordRect.x, passwordRect.y - 22);
        RenderText::drawText(renderer, "Confirm Password", fonts.small, muted, confirmRect.x, confirmRect.y - 22);
    }

    SDL_Color emailFill = activeField == Field::Email ? fieldActive : fieldFill;
    SDL_Color passFill = activeField == Field::Password ? fieldActive : fieldFill;
    SDL_Color confirmFill = activeField == Field::ConfirmPassword ? fieldActive : fieldFill;

    SDL_SetRenderDrawColor(renderer, emailFill.r, emailFill.g, emailFill.b, emailFill.a);
    SDL_RenderFillRect(renderer, &emailRect);
    SDL_SetRenderDrawColor(renderer, fieldOutline.r, fieldOutline.g, fieldOutline.b, fieldOutline.a);
    SDL_RenderDrawRect(renderer, &emailRect);

    SDL_SetRenderDrawColor(renderer, passFill.r, passFill.g, passFill.b, passFill.a);
    SDL_RenderFillRect(renderer, &passwordRect);
    SDL_SetRenderDrawColor(renderer, fieldOutline.r, fieldOutline.g, fieldOutline.b, fieldOutline.a);
    SDL_RenderDrawRect(renderer, &passwordRect);

    SDL_SetRenderDrawColor(renderer, confirmFill.r, confirmFill.g, confirmFill.b, confirmFill.a);
    SDL_RenderFillRect(renderer, &confirmRect);
    SDL_SetRenderDrawColor(renderer, fieldOutline.r, fieldOutline.g, fieldOutline.b, fieldOutline.a);
    SDL_RenderDrawRect(renderer, &confirmRect);

    if (fonts.small) {
        const int textPad = 10;
        RenderText::drawText(renderer, email, fonts.small, white, emailRect.x + textPad, emailRect.y + 12);
        std::string passMask(password.size(), '*');
        RenderText::drawText(renderer, passMask, fonts.small, white, passwordRect.x + textPad, passwordRect.y + 12);
        std::string confirmMask(confirmPassword.size(), '*');
        RenderText::drawText(renderer, confirmMask, fonts.small, white, confirmRect.x + textPad, confirmRect.y + 12);
    }

    std::string warningMessage = statusMessage;
    if (warningMessage.empty() && !confirmPassword.empty() && password != confirmPassword) {
        warningMessage = "Passwords do not match.";
    }

    if (fonts.small && !warningMessage.empty()) {
        const int messageY = confirmRect.y + confirmRect.h + 6;
        RenderText::drawText(renderer, warningMessage, fonts.small, errorRed, confirmRect.x, messageY);
    }

    SDL_Color baseButton{90, 90, 95, 255};
    SDL_Color highlightButton{115, 115, 120, 255};
    SDL_Color pressedButton{70, 70, 75, 255};
    SDL_Color backBase{52, 52, 56, 255};
    SDL_Color backHighlight{74, 74, 80, 255};
    SDL_Color backPressedColor{40, 40, 44, 255};

    RenderButton::drawButton(
        renderer,
        createButtonRect,
        "Create",
        createHover,
        false,
        baseButton,
        highlightButton,
        pressedButton,
        white,
        fonts.small
    );

    RenderButton::drawButton(
        renderer,
        backButtonRect,
        "Back",
        backHover,
        false,
        backBase,
        backHighlight,
        backPressedColor,
        white,
        fonts.small
    );
}
