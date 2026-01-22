#include "Game.hpp"
#include <iostream>
#include <stdexcept>

Game::Game(const char *title, int xpos, int ypos, int width, int height, bool fullscreen) {
    //Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
    }

    //Determine window type (fullscreen or not)
    Uint32 flags = fullscreen ? SDL_WINDOW_FULLSCREEN : 0;
    
    //Create window
    //reset destroys any old pointers that may exist for window
    window.reset(SDL_CreateWindow(title, xpos, ypos, width, height, flags));
    if (!window) {
        SDL_Quit();
        throw std::runtime_error(std::string("SDL_CreateWindow Failed: ") + SDL_GetError());
    }

    //Create renderer'
    renderer.reset(SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_ACCELERATED));
    if (!renderer) {
        SDL_Quit();
        throw std::runtime_error(std::string("SDL_CreateRenderer Failed: ") + SDL_GetError());
    }
    SDL_SetRenderDrawColor(renderer.get(), 0, 0, 0, 255);
    
    isRunning = true;
}

Game::~Game() {
    clean(); //quits SDL
}

void Game::handleEvents() {
    SDL_Event event;
    SDL_PollEvent(&event);
    switch(event.type) {
        case SDL_QUIT:
            isRunning = false;
            break;
        default:
            break;
    }
}

void Game::update() {}

void Game::render() {
    SDL_RenderClear(renderer.get());
    SDL_RenderPresent(renderer.get());
}

void Game::clean() {
    if (isRunning) {
        SDL_Quit();
        isRunning = false;
    }
}

bool Game::running() const {
    return isRunning;
}