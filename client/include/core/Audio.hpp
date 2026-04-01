#pragma once
#include <SDL2/SDL_mixer.h>
#include <string>
#include <unordered_map>
#include <algorithm>

class Audio {
public:
    // -------------------- MUSIC --------------------
    static void playMusic(const std::string& name);
    static void stopMusic();
    static void setMusicVolume(int volume); // 0-128
    static int  getMusicVolume();           // NEW — read current value for UI

    // -------------------- SFX --------------------
    static void loadSFX(const std::string& name);
    static void playSFX(const std::string& name);
    static void unloadSFX(const std::string& name);
    static void setSFXVolume(int volume);   // 0-128 global
    static int  getSFXVolume();             // NEW — read current value for UI
    static void setSFXOverride(const std::string& name, int volume);
    static void cleanup();

private:
    // -------------------- MUSIC --------------------
    static Mix_Music*   currentMusic;
    static std::string  currentTrack;
    static int          musicVolume;        // 0-128

    // -------------------- SFX --------------------
    static std::unordered_map<std::string, Mix_Chunk*> sfxMap;
    static int                                          sfxVolume; // 0-128
    static std::unordered_map<std::string, int>         sfxOverrides;

    // -------------------- PATH HELPERS --------------------
    static std::string musicPath(const std::string& name);
    static std::string sfxPath(const std::string& name);
};