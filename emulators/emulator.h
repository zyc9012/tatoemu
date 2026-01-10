#pragma once

#include "types.h"
#include "config.h"
#include "core.h"
#include <filesystem>
#include <memory>
#include <string>
#include <SDL3/SDL.h>

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

    bool loadBootrom(const fs::path& filename);
    bool loadROM(const fs::path& filename);
    void run();
    void runFrame();
    void shutdown();
    
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
    
    // Frame timing
    u64 m_lastFrameTime;
    double m_targetFPS;
    double m_gameSpeed = 1.0;
    double m_targetFrameTime;

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

