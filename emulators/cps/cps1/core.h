#pragma once

#include "../../types.h"
#include "../../core.h"
#include "../cpu.h"
#include "../sound_cpu.h"
#include "ppu.h"
#include "apu.h"
#include "memory.h"
#include "cartridge.h"
#include "../controller.h"
#include "../config.h"
#include "../consts.h"
#include "consts.h"
#include <filesystem>
#include <memory>
#include <string>

namespace cps1 {

class Core : public ::Core {
public:
    Core();
    ~Core() = default;

    // Core interface implementation
    bool initialize(VideoDevice* videoDevice, AudioDevice* audioDevice) override;
    bool loadROM(const fs::path& filename) override;
    bool handleInput(SDL_Event& event) override;
    void update() override;
    void updateGameSpeed(double gameSpeed) override;
    void setAudioSampleRate(u32 sampleRate) override { m_apu->setSampleRate(sampleRate); }
    void setAudioVolume(float volume) override { m_apu->setVolume(volume); }

    // Constants
    double getTargetFPS() const override { return TARGET_FPS; }
    u16 getScreenWidth() const override { return cps::SCREEN_WIDTH; }
    u16 getScreenHeight() const override { return cps::SCREEN_HEIGHT; }
    
    // Save/Load state
    bool saveState(const fs::path& filename) override;
    bool loadState(const fs::path& filename) override;

    const std::string& getGameTitle() const override { return m_cartridge->getTitle(); }
    
private:
    // Core components - using shared CPU and SoundCPU
    std::unique_ptr<cps::CPU> m_cpu;
    std::unique_ptr<cps::SoundCPU> m_soundCpu;
    std::unique_ptr<PPU> m_ppu;
    std::unique_ptr<APU> m_apu;
    std::unique_ptr<Memory> m_memory;
    std::unique_ptr<Cartridge> m_cartridge;
    std::unique_ptr<cps::Controller> m_controller1;
    std::unique_ptr<cps::Controller> m_controller2;
    
    // Timing
    u32 m_cpuCyclesThisFrame;
    u32 m_soundCpuCyclesThisFrame;
    
    // Frame timing
    double m_gameSpeed = 1.0;
};

} // namespace cps1
