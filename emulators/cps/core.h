#pragma once

#include "../../types.h"
#include "../../core.h"
#include "cpu.h"
#include "sound_cpu.h"
#include "ppu.h"
#include "apu.h"
#include "memory.h"
#include "cartridge.h"
#include "controller.h"
#include "config.h"
#include "consts.h"
#include <filesystem>
#include <memory>
#include <string>

namespace cps {

class Core : public ::Core {
public:
    Core();
    ~Core() = default;

    // Core interface implementation
    bool initialize() override;
    void setVideoDevice(::VideoDevice* videoDevice) override;
    void setAudioDevice(::AudioDevice* audioDevice) override;
    bool loadROM(const fs::path& filename) override;
    bool handleInput(SDL_Event& event) override;
    void update() override;
    void updateGameSpeed(double gameSpeed) override;
    void setAudioSampleRate(u32 sampleRate) override;
    void setAudioVolume(float volume) override;

    // Constants
    double getTargetFPS() const override { return cps::TARGET_FPS; }
    u16 getScreenWidth() const override { return isScreenVertical() ? cps::SCREEN_HEIGHT : cps::SCREEN_WIDTH; }
    u16 getScreenHeight() const override { return isScreenVertical() ? cps::SCREEN_WIDTH : cps::SCREEN_HEIGHT; }
    bool isScreenVertical() const { return m_cartridge->getGameInfo()->flags & GameFlags::GAME_FLAG_VERTICAL_SCREEN != 0; }
    
    // CPS games were designed for 4:3 CRT displays with non-square pixels
    // The internal resolution is 384x224, but should be displayed at 4:3 aspect ratio
    double getDisplayAspectRatio() const override { return isScreenVertical() ? 3.0 / 4.0 : 4.0 / 3.0; }
    
    // Save/Load state
    bool saveState(const fs::path& filename) override;
    bool loadState(const fs::path& filename) override;

    const std::string& getGameTitle() const override { return m_cartridge->getTitle(); }
    
private:
    // Core components
    std::unique_ptr<CPU> m_cpu;
    std::unique_ptr<SoundCPU> m_soundCpu;
    std::unique_ptr<PPU> m_ppu;
    std::unique_ptr<APU> m_apu;
    std::unique_ptr<Memory> m_memory;
    std::unique_ptr<Cartridge> m_cartridge;
    std::unique_ptr<Controller> m_controller;
    
    // Frame timing
    double m_gameSpeed = 1.0;
    
    // CPS version (1 or 2) - determined from loaded game
    u8 m_cpsVersion;
};

} // namespace cps
