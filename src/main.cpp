#include <SDL2/SDL.h>
#include <iostream>
#include "core/Game.hpp"


int main(int argv, char** args) {
    try {
        std::cout << "Beginning Game";
        
        Game game("SpeedCardGame", 100,100,800,600,false); //note that if set to true it just enlarges EVERYTHING, like a large zoom in
        while (game.running()) {
            game.handleEvents();
            game.update();
            game.render();
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    return 0;
}