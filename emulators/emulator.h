#pragma once

#include "types.h"
#include "config.h"
#include "gb/core.h"
#include <filesystem>
#include <memory>
#include <string>

class SDLVideoDevice : public VideoDevice {
public:
    SDLVideoDevice(SDL_Renderer* renderer, SDL_Texture* texture);
    ~SDLVideoDevice();

    void render(u32* buffer) override;

private:
    SDL_Renderer* m_renderer;
    SDL_Texture* m_texture;
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

    bool initialize();
    bool loadBootrom(const fs::path& filename);
    bool loadROM(const fs::path& filename);
    void run();
    void runFrame();
    void shutdown();
    
private:
    void handleInput();
    void updateWindowStats();
    void updateGameSpeed(double gameSpeed);

    // SDL components
    SDL_Window* m_window;
    SDL_Renderer* m_renderer;
    SDL_Texture* m_texture;

    // Core
    std::unique_ptr<gb::Core> m_core;
    
    // Video and audio devices
    std::unique_ptr<SDLVideoDevice> m_videoDevice;
    std::unique_ptr<SDLAudioDevice> m_audioDevice;
    
    bool m_running;
    bool m_paused;
    
    // Frame timing
    u64 m_lastFrameTime;
    double m_gameSpeed = 1.0;
    double m_targetFrameTime = 1000.0 / TARGET_FPS / m_gameSpeed;

    // Audio-driven synchronization
    // Audio buffer thresholds: maintain 1.5-4 frames worth of audio for smooth playback
    const int m_minAudioBufferSize = static_cast<int>((Config::Audio::SAMPLE_RATE * 2 * sizeof(float) / static_cast<double>(TARGET_FPS)) * 1.5);
    const int m_maxAudioBufferSize = static_cast<int>((Config::Audio::SAMPLE_RATE * 2 * sizeof(float) / static_cast<double>(TARGET_FPS)) * 4.0);
    
    // Speed adjustment for audio sync (1.0 = normal speed)
    double m_emulationSpeed;
    
    // Statistics for debugging (optional)
    u64 m_statsTimer;
    u64 m_frameCount;
    
    fs::path m_romFilename;
};

