#include "states/Login.hpp"

#include "core/Game.hpp"
#include "core/NetworkClient.hpp"
#include "utils/JsonUtil.hpp"
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
    constexpr std::size_t kMaxUsernameLen = 24;
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

    bool parseUserIdFromLoginResponse(const std::string& responseBody, int& outUserId, std::string& error) {
        std::string userJson;
        if (!JsonUtil::extractJsonObject(responseBody, "user", userJson)) {
            error = "missing user object";
            return false;
        }
        if (JsonUtil::readJsonIntField(userJson, "id", outUserId)) return true;
        if (JsonUtil::readJsonIntField(userJson, "ID", outUserId)) return true;
        error = "missing user id";
        return false;
    }

    bool authenticateUser(const std::string& email, const std::string& password, int& outUserId, std::string& error) {
        if (email.empty() || password.empty()) {
            error = "email and password required";
            return false;
        }

        const std::string host = getEnvOrDefault("AUTH_SERVICE_HOST", "127.0.0.1");
        const int port = getEnvIntOrDefault("AUTH_SERVICE_PORT", 8081);
        const std::string path = "/login";

        std::ostringstream payload;
        payload << "{\"email\":\"" << escapeJsonString(email)
                << "\",\"password\":\"" << escapeJsonString(password) << "\"}";

        int statusCode = -1;
        std::string responseBody;
        if (!sendHttpRequest(host, port, "POST", path, payload.str(), statusCode, responseBody)) {
            error = "auth service unreachable";
            return false;
        }

        if (statusCode != 200) {
            std::string responseError;
            if (JsonUtil::readJsonStringField(responseBody, "error", responseError)) {
                error = responseError;
            } else {
                error = "login failed";
            }
            return false;
        }

        if (!parseUserIdFromLoginResponse(responseBody, outUserId, error)) {
            return false;
        }

        return true;
    }
}

Login::Login() = default;

Login::~Login() {
    RenderText::closeFonts(fonts);
}

void Login::enter(Game& game) {
    ensureFonts();
    setActiveField(Field::Username);
    SDL_StartTextInput();
}

void Login::exit(Game& game) {
    SDL_StopTextInput();
    loginPressed = false;
    registerPressed = false;
    loginHover = false;
    backHover = false;
}

void Login::ensureFonts() {
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

bool Login::pointInRect(const SDL_Rect& rect, int x, int y) const {
    return x >= rect.x && x <= rect.x + rect.w && y >= rect.y && y <= rect.y + rect.h;
}

void Login::setActiveField(Field field) {
    activeField = field;
    if (activeField == Field::None) {
        SDL_StopTextInput();
    } else {
        SDL_StartTextInput();
    }
}

void Login::updateLayout(SDL_Renderer* renderer) {
    int screenW = 800;
    int screenH = 600;
    if (renderer) {
        SDL_GetRendererOutputSize(renderer, &screenW, &screenH);
    }

    const int panelW = std::min(560, static_cast<int>(screenW * 0.8f));
    const int panelH = 360;
    panelRect = SDL_Rect{
        (screenW - panelW) / 2,
        (screenH - panelH) / 2,
        panelW,
        panelH
    };

    const int inputW = panelW - 120;
    const int inputH = 44;
    const int inputX = panelRect.x + 60;
    int cursorY = panelRect.y + 80;

    usernameRect = SDL_Rect{inputX, cursorY, inputW, inputH};
    cursorY += inputH + 24;

    passwordRect = SDL_Rect{inputX, cursorY, inputW, inputH};
    cursorY += inputH + 32;

    const int buttonW = (inputW - 20) / 2;
    const int buttonH = 46;
    loginButtonRect = SDL_Rect{inputX, cursorY, buttonW, buttonH};
    backButtonRect = SDL_Rect{inputX + buttonW + 20, cursorY, buttonW, buttonH};
}

void Login::handleEvents(Game& game, const SDL_Event& event) {
    updateLayout(game.getRenderer());

    if (event.type == SDL_QUIT) {
        game.setNextState(GameState::Quit);
        return;
    }

    if (event.type == SDL_MOUSEMOTION) {
        const int mouseX = event.motion.x;
        const int mouseY = event.motion.y;
        loginHover = pointInRect(loginButtonRect, mouseX, mouseY);
        backHover = pointInRect(backButtonRect, mouseX, mouseY);
    }

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        const int mouseX = event.button.x;
        const int mouseY = event.button.y;

        if (pointInRect(usernameRect, mouseX, mouseY)) {
            setActiveField(Field::Username);
        } else if (pointInRect(passwordRect, mouseX, mouseY)) {
            setActiveField(Field::Password);
        } else {
            setActiveField(Field::None);
        }

        if (pointInRect(loginButtonRect, mouseX, mouseY)) {
            loginPressed = true;
        } else if (pointInRect(backButtonRect, mouseX, mouseY)) {
            registerPressed = true;
        }
    }

    if (event.type == SDL_TEXTINPUT) {
        if (activeField == Field::Username && username.size() < kMaxUsernameLen) {
            username.append(event.text.text);
        } else if (activeField == Field::Password && password.size() < kMaxPasswordLen) {
            password.append(event.text.text);
        }
    }

    if (event.type == SDL_KEYDOWN) {
        if (event.key.keysym.sym == SDLK_TAB) {
            if (activeField == Field::Username) {
                setActiveField(Field::Password);
            } else {
                setActiveField(Field::Username);
            }
        } else if (event.key.keysym.sym == SDLK_BACKSPACE) {
            if (activeField == Field::Username && !username.empty()) {
                username.pop_back();
            } else if (activeField == Field::Password && !password.empty()) {
                password.pop_back();
            }
        } else if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER) {
            loginPressed = true;
        } else if (event.key.keysym.sym == SDLK_ESCAPE) {
            registerPressed = true;
        }
    }
}

void Login::update(Game& game) {
    if (loginPressed) {
        std::cout << "Login requested for user: " << username << '\n';
        loginPressed = false;
        int userId = -1;
        std::string error;
        if (!authenticateUser(username, password, userId, error)) {
            std::cerr << "Login failed: " << error << '\n';
            game.setNextState(GameState::Register);
            return;
        }
        game.setPlayerId(userId);
        if (!game.refreshPlayerDeckFromService()) {
            std::cerr << "Failed to refresh deck data after login\n";
        }
        game.setNextState(GameState::Title);
        return;
    }

    if (registerPressed) {
        registerPressed = false;
        game.setNextState(GameState::Register);
    }
}

void Login::render(Game& game) {
    SDL_Renderer* renderer = game.getRenderer();
    updateLayout(renderer);
    ensureFonts();

    SDL_SetRenderDrawColor(renderer, 18, 18, 20, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 40, 45, 60, 230);
    SDL_RenderFillRect(renderer, &panelRect);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &panelRect);

    SDL_Color white{245, 245, 245, 255};
    SDL_Color muted{190, 190, 190, 255};
    SDL_Color fieldFill{30, 30, 34, 255};
    SDL_Color fieldActive{45, 45, 55, 255};

    if (fonts.large) {
        RenderText::drawText(renderer, "Login", fonts.large, white, panelRect.x + 30, panelRect.y + 20);
    }

    if (fonts.small) {
        RenderText::drawText(renderer, "Username", fonts.small, muted, usernameRect.x, usernameRect.y - 22);
        RenderText::drawText(renderer, "Password", fonts.small, muted, passwordRect.x, passwordRect.y - 22);
    }

    SDL_Color userFill = activeField == Field::Username ? fieldActive : fieldFill;
    SDL_Color passFill = activeField == Field::Password ? fieldActive : fieldFill;

    SDL_SetRenderDrawColor(renderer, userFill.r, userFill.g, userFill.b, userFill.a);
    SDL_RenderFillRect(renderer, &usernameRect);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &usernameRect);

    SDL_SetRenderDrawColor(renderer, passFill.r, passFill.g, passFill.b, passFill.a);
    SDL_RenderFillRect(renderer, &passwordRect);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &passwordRect);

    if (fonts.small) {
        const int textPad = 10;
        RenderText::drawText(renderer, username, fonts.small, white, usernameRect.x + textPad, usernameRect.y + 12);
        std::string mask(password.size(), '*');
        RenderText::drawText(renderer, mask, fonts.small, white, passwordRect.x + textPad, passwordRect.y + 12);
    }

    SDL_Color baseButton{70, 120, 200, 255};
    SDL_Color highlightButton{90, 150, 220, 255};
    SDL_Color pressedButton{60, 100, 180, 255};
    SDL_Color backBase{70, 70, 70, 255};
    SDL_Color backHighlight{90, 90, 90, 255};
    SDL_Color registerPressed{60, 60, 60, 255};

    RenderButton::drawButton(
        renderer,
        loginButtonRect,
        "Login",
        loginHover,
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
        "Register",
        backHover,
        false,
        backBase,
        backHighlight,
        registerPressed,
        white,
        fonts.small
    );
}
