#pragma once

#include "types.h"
#include "../core.h"
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
    // ICheatMemory adapter — 8-bit bus; multi-byte ops are little-endian.
    struct CheatMemory : ICheatMemory {
        gb::MMU* mmu = nullptr;
        u8   peek8 (u32 a) override { return mmu->read(static_cast<u16>(a)); }
        void poke8 (u32 a, u8  v) override { mmu->write(static_cast<u16>(a), v); }
        std::vector<MemRegion> getSearchRegions() const override {
            return {
                { 0xC000, 0x2000 },  // Work RAM (8 KB, covers DMG + GBC bank 0+1)
                { 0xA000, 0x2000 },  // Cartridge RAM (8 KB window)
            };
        }
    } m_cheatMem;


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

