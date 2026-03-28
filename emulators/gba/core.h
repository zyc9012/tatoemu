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
    struct CheatMemory : ICheatMemory {
        gba::Memory* mem = nullptr;
        u8  peek8 (u32 addr) override { return mem->read8(addr, true); }
        u16 peek16(u32 addr) override { return mem->read16(addr, false, true); }
        u32 peek32(u32 addr) override { return mem->read32(addr, false, true); }
        void poke8 (u32 addr, u8  v) override { mem->write8(addr, v, true); }
        void poke16(u32 addr, u16 v) override { mem->write16(addr, v, true); }
        void poke32(u32 addr, u32 v) override { mem->write32(addr, v, true); }
        std::vector<MemRegion> getSearchRegions() const override {
            return {
                { 0x02000000, EWRAM_SIZE },   // External Work RAM (256 KB)
                { 0x03000000, IWRAM_SIZE },   // Internal Work RAM  (32 KB)
                { 0x0E000000, SRAM_SIZE  },   // Cartridge SRAM/Flash (64 KB)
            };
        }
    } m_cheatMem;


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
