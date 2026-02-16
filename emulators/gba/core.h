#pragma once

#include "types.h"
#include "../core.h"
#include "cpu.h"
#include "memory.h"
#include "ppu.h"
#include "joypad.h"
#include "timer.h"
#include "dma.h"
#include "apu.h"
#include "cartridge.h"
#include "gpio.h"
#include "config.h"
#include <filesystem>
#include <memory>
#include <string>

namespace gba {

class Core : public ::Core {
public:
    Core();
    ~Core() = default;

    // Core interface implementation
    bool initialize() override;
    void setVideoDevice(VideoDevice* videoDevice) override;
    void setAudioDevice(AudioDevice* audioDevice) override;
    bool loadBootrom(const fs::path& filename) override;
    bool loadROM(const fs::path& filename) override;
    bool handleInput(SDL_Event& event) override { return m_joypad->handleInput(event); }
    void update() override;
    void updateGameSpeed(double gameSpeed) override;
    void setAudioSampleRate(u32 sampleRate) override { if (m_apu) m_apu->setSampleRate(sampleRate); }
    void setAudioVolume(float volume) override { if (m_apu) m_apu->setVolume(volume); }

    // Constants
    double getTargetFPS() const override { return TARGET_FPS; }
    u16 getScreenWidth() const override { return SCREEN_WIDTH; }
    u16 getScreenHeight() const override { return SCREEN_HEIGHT; }
    
    // Save/Load state
    bool saveState(const fs::path& filename) override;
    bool loadState(const fs::path& filename) override;

    const std::string& getGameTitle() const override { return m_cartridge->getTitle(); }
    
private:
    // Core components
    std::unique_ptr<CPU> m_cpu;
    std::unique_ptr<Memory> m_memory;
    std::unique_ptr<PPU> m_ppu;
    std::unique_ptr<Joypad> m_joypad;
    std::unique_ptr<Timer> m_timer;
    std::unique_ptr<DMA> m_dma;
    std::unique_ptr<APU> m_apu;
    std::unique_ptr<Cartridge> m_cartridge;
    std::unique_ptr<GPIO> m_gpio;
    
    u32 m_cyclesThisFrame;
    
    // Frame timing
    double m_gameSpeed = 1.0;
};

} // namespace gba
