#pragma once

#include "types.h"
#include "cpu.h"
#include "mmu.h"
#include "ppu.h"
#include "joypad.h"
#include "timer.h"
#include "cartridge.h"
#include "apu.h"
#include "bootrom.h"
#include "config.h"
#include <filesystem>
#include <memory>
#include <string>

namespace fs = std::filesystem;

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;
struct SDL_AudioStream;

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
    bool loadBootrom(const std::string& filename);
    bool loadROM(const std::string& filename);
    void run();
    void runFrame();
    void shutdown();
    
    // Save/Load state
    void saveState(const std::string& filename);
    void loadState(const std::string& filename);
    
private:
    void handleInput();
    void update();
    void updateWindowStats();

    // SDL components
    SDL_Window* m_window;
    SDL_Renderer* m_renderer;
    SDL_Texture* m_texture;
    
    // Emulator components
    std::unique_ptr<CPU> m_cpu;
    std::unique_ptr<MMU> m_mmu;
    std::unique_ptr<PPU> m_ppu;
    std::unique_ptr<Joypad> m_joypad;
    std::unique_ptr<Timer> m_timer;
    std::unique_ptr<Cartridge> m_cartridge;
    std::unique_ptr<APU> m_apu;
    std::unique_ptr<Bootrom> m_bootrom;
    std::unique_ptr<SDLVideoDevice> m_videoDevice;
    std::unique_ptr<SDLAudioDevice> m_audioDevice;
    
    bool m_running;
    bool m_paused;
    u32 m_cyclesThisFrame;
    
    // Frame timing
    u64 m_lastFrameTime;
    const double m_targetFrameTime = 1000.0 / TARGET_FPS;

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

