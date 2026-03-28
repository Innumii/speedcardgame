#ifndef LOGIN_HPP
#define LOGIN_HPP

#include <SDL2/SDL.h>
#include <string>

#include "StateInterface.hpp"
#include "render/RenderText.hpp"

class Game;

class Login : public StateInterface {
public:
    Login();
    ~Login();

    void enter(Game& game) override;
    void exit(Game& game) override;
    void handleEvents(Game& game, const SDL_Event& event) override;
    void update(Game& game) override;
    void render(Game& game) override;

private:
    enum class Field {
        None,
        Username,
        Password
    };

    void updateLayout(SDL_Renderer* renderer);
    void setActiveField(Field field);

    SDL_Rect panelRect{0, 0, 0, 0};
    SDL_Rect usernameRect{0, 0, 0, 0};
    SDL_Rect passwordRect{0, 0, 0, 0};
    SDL_Rect loginButtonRect{0, 0, 0, 0};
    SDL_Rect backButtonRect{0, 0, 0, 0};

    Field activeField{Field::Username};
    std::string username;
    std::string password;
    std::string statusMessage; // add auth status msg

    bool loginPressed{false};
    bool registerPressed{false};
    bool loginHover{false};
    bool backHover{false};

    float cachedScale{1.0f};
    int cachedBannerY{30};
    int cachedBannerW{0};
    int cachedBannerH{0};
    int cachedLabelOffY{30};
    int cachedInputPad{16};

};

#endif
