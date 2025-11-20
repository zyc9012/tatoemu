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

namespace gb {

class Core {
public:
    Core();
    ~Core() = default;

    bool initialize(VideoDevice* videoDevice, AudioDevice* audioDevice);
    bool loadBootrom(const fs::path& filename);
    bool loadROM(const fs::path& filename);
    bool handleInput(SDL_Event& event);
    void update();
    void updateGameSpeed(double gameSpeed);
    void setAudioSampleRate(u32 sampleRate) { m_apu->setSampleRate(sampleRate); }
    void setAudioVolume(float volume) { m_apu->setVolume(volume); }

    // Constants
    double getTargetFPS() const { return TARGET_FPS; }
    u16 getScreenWidth() const { return SCREEN_WIDTH; }
    u16 getScreenHeight() const { return SCREEN_HEIGHT; }
    
    // Save/Load state
    bool saveState(const fs::path& filename);
    bool loadState(const fs::path& filename);

    const std::string& getGameTitle() const { return m_cartridge->getTitle(); }
    
private:
    // Core components
    std::unique_ptr<CPU> m_cpu;
    std::unique_ptr<MMU> m_mmu;
    std::unique_ptr<PPU> m_ppu;
    std::unique_ptr<Joypad> m_joypad;
    std::unique_ptr<Timer> m_timer;
    std::unique_ptr<Cartridge> m_cartridge;
    std::unique_ptr<APU> m_apu;
    std::unique_ptr<Bootrom> m_bootrom;
    
    u32 m_cyclesThisFrame;
    
    // Frame timing
    double m_gameSpeed = 1.0;
};

} // namespace gb

