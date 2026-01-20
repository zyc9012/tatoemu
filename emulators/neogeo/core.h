#pragma once

#include "../../types.h"
#include "../../core.h"
#include "cpu.h"
#include "sound_cpu.h"
#include "apu.h"
#include "ppu.h"
#include "memory.h"
#include "cartridge.h"
#include "controller.h"
#include "config.h"
#include "consts.h"
#include <filesystem>
#include <memory>
#include <string>

namespace neogeo {

class Core : public ::Core {
public:
    Core();
    ~Core() = default;

    // Core interface implementation
    bool initialize() override;
    void setVideoDevice(::VideoDevice* videoDevice) override;
    void setAudioDevice(::AudioDevice* audioDevice) override;
    bool loadROM(const fs::path& filename) override;

    // NeoGeo-specific methods
    void setBIOSIndex(u32 bios68kIndex) { m_bios68kIndex = bios68kIndex; }
    u32 getBIOSIndex() const { return m_bios68kIndex; }
    bool isAES() const { return m_cartridge->isAES(); }
    bool handleInput(SDL_Event& event) override;
    void update() override;
    void updateGameSpeed(double gameSpeed) override;
    void setAudioSampleRate(u32 sampleRate) override;
    void setAudioVolume(float volume) override;

    // Constants
    double getTargetFPS() const override { return neogeo::TARGET_FPS; }
    u16 getScreenWidth() const override { return m_cartridge->getGameInfo()->screenWidth; }
    u16 getScreenHeight() const override { return m_cartridge->getGameInfo()->screenHeight; }
    
    // NeoGeo games were designed for 4:3 CRT displays
    double getDisplayAspectRatio() const override { return 4.0 / 3.0; }
    
    // Save/Load state
    bool saveState(const fs::path& filename) override;
    bool loadState(const fs::path& filename) override;

    const std::string& getGameTitle() const override { return m_cartridge->getTitle(); }
    
private:
    // Core components
    std::unique_ptr<CPU> m_cpu;
    std::unique_ptr<SoundCPU> m_soundCpu;
    std::unique_ptr<APU> m_apu;
    std::unique_ptr<PPU> m_ppu;
    std::unique_ptr<Memory> m_memory;
    std::unique_ptr<Cartridge> m_cartridge;
    std::unique_ptr<Controller> m_controller;
    
    // Frame timing
    double m_gameSpeed = 1.0;

    // BIOS configuration
    u32 m_bios68kIndex = 19;
};

} // namespace neogeo
