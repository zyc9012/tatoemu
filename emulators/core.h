#pragma once

#include "types.h"
#include "cheat.h"
#include <filesystem>
#include <string>
#include <SDL3/SDL.h>

// Abstract base class for all emulator cores
class Core {
public:
    virtual ~Core() = default;

    // Initialization
    virtual bool initialize() = 0;

    // Device configuration (can be called after initialize)
    virtual void setVideoDevice(VideoDevice* videoDevice) = 0;
    virtual void setAudioDevice(AudioDevice* audioDevice) = 0;
    virtual bool loadROM(const fs::path& filename) = 0;
    
    // Bootrom loading (optional, GB only - default empty implementation)
    virtual bool loadBootrom(const fs::path& filename) { 
        (void)filename; 
        return true; 
    }
    
    // Input handling
    virtual bool handleInput(SDL_Event& event) = 0;
    
    // Emulation loop
    virtual void update() = 0;
    virtual void updateGameSpeed(double gameSpeed) = 0;
    
    // Audio configuration
    virtual void setAudioSampleRate(u32 sampleRate) = 0;
    virtual void setAudioVolume(float volume) = 0;
    
    // Screen information
    virtual double getTargetFPS() const = 0;
    virtual u16 getScreenWidth() const = 0;
    virtual u16 getScreenHeight() const = 0;
    
    // Display aspect ratio (defaults to pixel aspect ratio if not overridden)
    // This represents the intended display aspect ratio on the original hardware,
    // which may differ from pixel aspect ratio due to non-square pixels on CRTs
    virtual double getDisplayAspectRatio() const {
        return static_cast<double>(getScreenWidth()) / static_cast<double>(getScreenHeight());
    }
    
    // Save/Load state
    virtual bool saveState(const fs::path& filename) = 0;
    virtual bool loadState(const fs::path& filename) = 0;
    
    // Cheat / memory-search support.
    virtual ICheatMemory* getCheatMemory() { return nullptr; }
    
    // Game information
    virtual const std::string& getGameTitle() const = 0;
};

