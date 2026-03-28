#pragma once

#include "../types.h"
#include "../core.h"
#include "cpu.h"
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

namespace nes {

class Core : public ::Core {
public:
    Core();
    ~Core() = default;

    // Core interface implementation
    bool initialize() override;
    void setVideoDevice(VideoDevice* videoDevice) override;
    void setAudioDevice(AudioDevice* audioDevice) override;
    bool loadROM(const fs::path& filename) override;
    bool handleInput(SDL_Event& event) override { return m_controller->handleInput(event); }
    void update() override;
    void updateGameSpeed(double gameSpeed) override;
    void setAudioSampleRate(u32 sampleRate) override { m_apu->setSampleRate(sampleRate); }
    void setAudioVolume(float volume) override { m_apu->setVolume(volume); }

    // Constants
    double getTargetFPS() const override { return TARGET_FPS; }
    u16 getScreenWidth() const override { return SCREEN_WIDTH; }
    u16 getScreenHeight() const override { return SCREEN_HEIGHT; }
    
    // Save/Load state
    bool saveState(const fs::path& filename) override;
    bool loadState(const fs::path& filename) override;

    const std::string& getGameTitle() const override { return m_cartridge->getTitle(); }

    ICheatMemory* getCheatMemory() override { return &m_cheatMem; }

private:
    // ICheatMemory adapter — 8-bit bus; multi-byte ops are little-endian.
    struct CheatMemory : ICheatMemory {
        nes::Memory* mem = nullptr;
        u8   peek8 (u32 a) override { return mem->cpuRead(static_cast<u16>(a)); }
        void poke8 (u32 a, u8  v) override { mem->cpuWrite(static_cast<u16>(a), v); }
        std::vector<MemRegion> getSearchRegions() const override {
            return { { 0x0000, RAM_SIZE } };  // 2 KB internal RAM
        }
    } m_cheatMem;


    // Core components
    std::unique_ptr<CPU> m_cpu;
    std::unique_ptr<PPU> m_ppu;
    std::unique_ptr<APU> m_apu;
    std::unique_ptr<Memory> m_memory;
    std::unique_ptr<Cartridge> m_cartridge;
    std::unique_ptr<Controller> m_controller;

    u32 m_cyclesThisFrame;
    
    // Frame timing
    double m_gameSpeed = 1.0;
};

} // namespace nes
