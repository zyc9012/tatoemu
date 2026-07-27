#pragma once

#include "consts.h"
#include "config.h"
#include "../core.h"
#include "../types.h"
#include "cpu.h"
#include "sound_cpu.h"
#include "memory.h"
#include "vdp.h"
#include "audio.h"
#include "cartridge.h"
#include "controller.h"
#include <memory>
#include <string>

namespace md {

class Core : public ::Core {
public:
    Core();
    ~Core() = default;

    bool initialize() override;
    void setVideoDevice(::VideoDevice* videoDevice) override;
    void setAudioDevice(::AudioDevice* audioDevice) override;
    bool loadROM(const fs::path& filename) override;
    bool handleInput(SDL_Event& event) override { return m_controller->handleInput(event); }
    void update() override;
    void updateGameSpeed(double gameSpeed) override { m_gameSpeed = gameSpeed; }
    void setAudioSampleRate(u32 sampleRate) override { m_audio->setSampleRate(sampleRate); }
    void setAudioVolume(float volume) override { m_audio->setVolume(volume); }

    double getTargetFPS() const override { return m_vdp->targetFPS(); }
    u16 getScreenWidth() const override { return SCREEN_WIDTH; }
    u16 getScreenHeight() const override { return SCREEN_HEIGHT; }
    double getDisplayAspectRatio() const override { return 4.0 / 3.0; }

    bool saveState(const fs::path& filename) override;
    bool loadState(const fs::path& filename) override;

    const std::string& getGameTitle() const override { return m_cartridge->getTitle(); }

    ICheatMemory* getCheatMemory() override { return &m_cheatMem; }

private:
    void reset();
    // Advances the 68000 to the given frame cycle position and keeps the Z80
    // and the FM timers in step with it.
    void runTo(u32 target68kCycles);

    struct CheatMemory : ICheatMemory {
        md::Memory* mem = nullptr;
        u8  peek8 (u32 addr) override { return mem->read8(addr); }
        u16 peek16(u32 addr) override { return mem->read16(addr); }
        u32 peek32(u32 addr) override {
            return (static_cast<u32>(mem->read16(addr)) << 16) | mem->read16(addr + 2);
        }
        void poke8 (u32 addr, u8  v) override { mem->write8(addr, v); }
        void poke16(u32 addr, u16 v) override { mem->write16(addr, v); }
        void poke32(u32 addr, u32 v) override {
            mem->write16(addr, static_cast<u16>(v >> 16));
            mem->write16(addr + 2, static_cast<u16>(v & 0xFFFF));
        }
        std::vector<MemRegion> getSearchRegions() const override {
            return { { 0xFF0000, WORK_RAM_SIZE } };
        }
    } m_cheatMem;

    std::unique_ptr<CPU> m_cpu;
    std::unique_ptr<SoundCPU> m_soundCpu;
    std::unique_ptr<Memory> m_memory;
    std::unique_ptr<VDP> m_vdp;
    std::unique_ptr<Audio> m_audio;
    std::unique_ptr<Cartridge> m_cartridge;
    std::unique_ptr<Controller> m_controller;

    double m_gameSpeed = 1.0;

    // Z80 interrupt pulse bookkeeping.
    bool m_z80IrqAsserted = false;
};

} // namespace md
