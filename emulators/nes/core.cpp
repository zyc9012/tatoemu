#include "core.h"
#include "../components/buffer.h"
#include <SDL3/SDL.h>

namespace nes {

Core::Core()
    : m_cpuCycleCarry(0) {
}

bool Core::initialize() {
    // Create core components
    m_cartridge = std::make_unique<Cartridge>();
    m_cpu = std::make_unique<CPU>();
    m_ppu = std::make_unique<PPU>();
    m_apu = std::make_unique<APU>();
    m_memory = std::make_unique<Memory>();
    m_controller = std::make_unique<Controller>();

    // Wire up components
    m_cpu->setMemory(m_memory.get());
    
    m_ppu->setCPU(m_cpu.get());
    m_ppu->setCartridge(m_cartridge.get());
    
    m_apu->setCPU(m_cpu.get());
    m_apu->setMemory(m_memory.get());
    m_apu->setCartridge(m_cartridge.get());
    
    m_memory->setCPU(m_cpu.get());
    m_memory->setPPU(m_ppu.get());
    m_memory->setAPU(m_apu.get());
    m_memory->setCartridge(m_cartridge.get());
    m_memory->setController(m_controller.get());
    
    m_cartridge->setCPU(m_cpu.get());
    m_cartridge->setPPU(m_ppu.get());

    return true;
}

void Core::setVideoDevice(VideoDevice* videoDevice) {
    if (m_ppu) {
        m_ppu->setVideoDevice(videoDevice);
    }
}

void Core::setAudioDevice(AudioDevice* audioDevice) {
    if (m_apu) {
        m_apu->setAudioDevice(audioDevice);
    }
}

bool Core::loadROM(const fs::path& filename) {
    if (!m_cartridge->load(filename)) {
        return false;
    }

    // Reset all components
    m_cpu->reset();
    m_ppu->reset();
    m_apu->reset();
    m_memory->reset();
    m_controller->reset();
    m_cartridge->reset();

    return true;
}

void Core::update() {
    // Scanline-based rendering: run CPU for one scanline's worth of cycles,
    // then process the scanline in the PPU.

    for (u32 scanline = 0; scanline < SCANLINES_PER_FRAME; scanline++) {
        // Exact CPU cycles for this scanline using integer division:
        // total PPU cycles through end of this scanline vs start of this scanline
        u32 cpuCyclesNeeded = ((scanline + 1) * CYCLES_PER_SCANLINE / PPU_CYCLES_PER_CPU)
                            - (scanline * CYCLES_PER_SCANLINE / PPU_CYCLES_PER_CPU);

        while (m_cpuCycleCarry < cpuCyclesNeeded) {
            u32 cyclesBefore = m_cpu->getCycles();
            m_cpu->step(1);
            u32 cpuCycles = m_cpu->getCycles() - cyclesBefore;
            m_cpuCycleCarry += cpuCycles;

            // Run APU
            m_apu->step(cpuCycles, m_gameSpeed);

            // Check for mapper IRQ (VRC6 and similar)
            if (m_cartridge->irqState()) {
                m_cpu->irq();
                m_cartridge->irqClear();
            }
        }

        // Carry over excess cycles to the next scanline
        m_cpuCycleCarry -= cpuCyclesNeeded;

        // Process this scanline in the PPU (renders entire scanline at once)
        m_ppu->stepScanline();
    }
}

bool Core::saveState(const fs::path& filename) {
    Buffer buf = {};
    
    // Save all component states
    buffer_write(&buf, &m_cpuCycleCarry, sizeof(m_cpuCycleCarry));
    m_cpu->saveState(&buf);
    m_ppu->saveState(&buf);
    m_apu->saveState(&buf);
    m_memory->saveState(&buf);
    m_cartridge->saveState(&buf);
    m_controller->saveState(&buf);
    
    return buffer_save_to_file(&buf, filename);
}

bool Core::loadState(const fs::path& filename) {
    Buffer buf = {};
    buffer_load_from_file(&buf, filename);
    
    // Load all component states
    buffer_read(&buf, &m_cpuCycleCarry, sizeof(m_cpuCycleCarry));
    m_cpu->loadState(&buf);
    m_ppu->loadState(&buf);
    m_apu->loadState(&buf);
    m_memory->loadState(&buf);
    m_cartridge->loadState(&buf);
    m_controller->loadState(&buf);
    
    return true;
}

void Core::updateGameSpeed(double gameSpeed) {
    m_gameSpeed = gameSpeed;
}

} // namespace nes
