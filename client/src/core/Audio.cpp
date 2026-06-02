#include "core/Audio.hpp"
#include <iostream>

Mix_Music* Audio::currentMusic = nullptr;
std::string Audio::currentTrack = "";
std::unordered_map<std::string, Mix_Chunk*> Audio::sfxMap;

// Global volume settings
int Audio::musicVolume = 128; // 0-128
int Audio::sfxVolume = 128;   // 0-128
std::unordered_map<std::string, int> Audio::sfxOverrides; // per-sfx volume

// -------------------- PATHS --------------------
std::string Audio::musicPath(const std::string& name) {
    return "assets/music/" + name + ".mp3";
}

std::string Audio::sfxPath(const std::string& name) {
    return "assets/sfx/" + name + ".wav";
}

// -------------------- MUSIC --------------------
void Audio::playMusic(const std::string& name) {
    std::string path = musicPath(name);
    if (currentTrack == path) return;

    if (currentMusic) {
        Mix_HaltMusic();
        Mix_FreeMusic(currentMusic);
        currentMusic = nullptr;
    }

    currentMusic = Mix_LoadMUS(path.c_str());
    if (!currentMusic) {
        std::cout << "Music load error: " << Mix_GetError() << " (" << path << ")\n";
        return;
    }

    Mix_VolumeMusic(musicVolume); // apply current music volume
    Mix_PlayMusic(currentMusic, -1);
    currentTrack = path;
}

void Audio::stopMusic() {
    if (currentMusic) {
        Mix_HaltMusic();
        Mix_FreeMusic(currentMusic);
        currentMusic = nullptr;
        currentTrack = "";
    }
}

void Audio::setMusicVolume(int volume) {
    musicVolume = std::clamp(volume, 0, 128);
    Mix_VolumeMusic(musicVolume);
}

// -------------------- SFX --------------------
void Audio::loadSFX(const std::string& name) {
    if (sfxMap.count(name)) return; // already loaded

    Mix_Chunk* chunk = Mix_LoadWAV(sfxPath(name).c_str());
    if (!chunk) {
        std::cout << "SFX load error: " << Mix_GetError() << " (" << name << ")\n";
        return;
    }

    sfxMap[name] = chunk;
    Mix_VolumeChunk(chunk, sfxVolume); // apply global volume initially
}

void Audio::playSFX(const std::string& name) {
    if (!sfxMap.count(name)) {
        loadSFX(name); // auto-load if not loaded
    }

    if (sfxMap.count(name)) {
        int volume = sfxVolume;
        if (sfxOverrides.count(name)) {
            volume = std::clamp(sfxOverrides[name], 0, 128);
        }

        int channel = Mix_PlayChannel(-1, sfxMap[name], 0);
        if (channel != -1) {
            Mix_Volume(channel, volume); // per-playback volume
        }
    }
}

void Audio::setSFXVolume(int volume) {
    sfxVolume = std::clamp(volume, 0, 128);
    for (auto& [name, chunk] : sfxMap) {
        Mix_VolumeChunk(chunk, sfxVolume);
    }
}

void Audio::setSFXOverride(const std::string& name, int volume) {
    sfxOverrides[name] = std::clamp(volume, 0, 128);
}

// -------------------- CLEANUP --------------------
void Audio::unloadSFX(const std::string& name) {
    if (sfxMap.count(name)) {
        Mix_FreeChunk(sfxMap[name]);
        sfxMap.erase(name);
        sfxOverrides.erase(name);
    }
}

void Audio::cleanup() {
    stopMusic();
    for (auto& [name, chunk] : sfxMap) {
        Mix_FreeChunk(chunk);
    }
    sfxMap.clear();
    sfxOverrides.clear();
    Mix_CloseAudio();
    Mix_Quit();
}

int Audio::getMusicVolume() {
    return musicVolume;
}
 
int Audio::getSFXVolume() {
    return sfxVolume;
}
 