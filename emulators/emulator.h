#pragma once

#include "types.h"
#include "config.h"
#include "core.h"
#include "cheat.h"
#include "frame_pacer.h"
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <SDL3/SDL.h>

enum class CoreType {
    GB,
    GBA,
    NES,
    CPS,
    NEOGEO,
    UNKNOWN
};

class SDLVideoDevice : public VideoDevice {
public:
    SDLVideoDevice(SDL_Renderer* renderer, SDL_Texture* texture, u16 screenWidth, u16 screenHeight);
    ~SDLVideoDevice();

    void render(u32* buffer) override;
    void setDisplayAspectRatio(double aspectRatio) { m_displayAspectRatio = aspectRatio; }

private:
    SDL_Renderer* m_renderer;
    SDL_Texture* m_texture;
    u16 m_screenWidth;
    double m_displayAspectRatio;  // Display aspect ratio (may differ from pixel aspect ratio)
};

class SDLAudioDevice : public AudioDevice {
public:
    SDLAudioDevice();
    ~SDLAudioDevice();

    bool initialize();
    void shutdown();
    void clearBuffer();
    int getQueuedSize() const;

    void writeSamples(void* stream, u32 length) override;

private:
    SDL_AudioStream* m_audioStream;
};

class Emulator {
public:
    Emulator();
    ~Emulator();

    static CoreType determineCoreType(const fs::path& filename);

    bool loadBootrom(const fs::path& filename);
    bool loadROM(const fs::path& filename);
    void run();
    void runFrame();
    bool handleKeyInput(SDL_Keycode keycode, bool pressed);
    void shutdown();

    // Called once at the end of every runFrame(), before timing sleep.
    // Use this to drain platform-specific per-frame work (e.g. cheat console).
    void setFrameCallback(std::function<void()> cb) { m_frameCallback = std::move(cb); }

    CheatEngine& getCheatEngine() { return m_cheatEngine; }
    MemSearcher&  getSearcher()   { return m_searcher; }
    fs::path      getRomPath()    const { return m_romFilename; }
    
private:
    bool initialize();
    void handleInput();
    void updateWindowStats();
    void updateGameSpeed(double gameSpeed);

    // SDL components
    SDL_Window* m_window;
    SDL_Renderer* m_renderer;
    SDL_Texture* m_texture;

    // Core - abstract base class supports all emulator types
    std::unique_ptr<Core> m_core;
    
    // Video and audio devices
    std::unique_ptr<SDLVideoDevice> m_videoDevice;
    std::unique_ptr<SDLAudioDevice> m_audioDevice;
    
    bool m_running;
    bool m_paused;

    // Per-frame callback (optional, set by platform layer).
    std::function<void()> m_frameCallback;
    
    // Cheat engine and memory searcher (per loaded ROM).
    CheatEngine m_cheatEngine;
    MemSearcher m_searcher;
    
    // Frame timing
    u64 m_lastFrameTime;
    double m_targetFPS;
    double m_gameSpeed = 1.0;
    double m_targetFrameTime;
    FramePacer m_pacer;

    // Audio-driven synchronization
    int m_minAudioBufferSize;
    int m_maxAudioBufferSize;
    
    // Speed adjustment for audio sync (1.0 = normal speed)
    double m_emulationSpeed;
    
    // Statistics for debugging (optional)
    u64 m_statsTimer;
    u64 m_frameCount;
    
    fs::path m_bootromFilename;
    fs::path m_romFilename;
};

