#pragma once
#include <SDL2/SDL_mixer.h>
#include <string>
#include <unordered_map>
#include <algorithm>

class Audio {
public:
    // -------------------- MUSIC --------------------
    static void playMusic(const std::string& name); // e.g., "menu"
    static void stopMusic();
    static void setMusicVolume(int volume);        // 0-128

    // -------------------- SFX --------------------
    static void loadSFX(const std::string& name);  // preload
    static void playSFX(const std::string& name);  // play
    static void unloadSFX(const std::string& name); // free a sound effect
    static void setSFXVolume(int volume);          // global volume
    static void setSFXOverride(const std::string& name, int volume); // per-SFX volume
    static void cleanup();                          // free everything

private:
    // -------------------- MUSIC --------------------
    static Mix_Music* currentMusic;
    static std::string currentTrack;
    static int musicVolume; // 0-128

    // -------------------- SFX --------------------
    static std::unordered_map<std::string, Mix_Chunk*> sfxMap;
    static int sfxVolume; // 0-128
    static std::unordered_map<std::string, int> sfxOverrides; // per-sfx volume

    // -------------------- PATH HELPERS --------------------
    static std::string musicPath(const std::string& name);
    static std::string sfxPath(const std::string& name);
};