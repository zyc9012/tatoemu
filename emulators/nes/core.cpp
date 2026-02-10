#include "core.h"
#include "../components/buffer.h"
#include <SDL3/SDL.h>

namespace nes {

Core::Core()
    : m_cyclesThisFrame(0) {
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
    while (m_cyclesThisFrame < CPU_CYCLES_PER_FRAME) {
        // Track cycles before instruction
        u32 cyclesBefore = m_cpu->getCycles();

        // Execute one CPU instruction (takes multiple cycles)
        m_cpu->step(1);

        // Calculate how many CPU cycles the instruction took
        u32 cpuCycles = m_cpu->getCycles() - cyclesBefore;
        m_cyclesThisFrame += cpuCycles;

        // Run PPU for 3 PPU cycles per CPU cycle
        u32 ppuCycles = cpuCycles * PPU_CYCLES_PER_CPU;
        for (u32 i = 0; i < ppuCycles; i++) {
            m_ppu->step();
        }

        // Run APU
        m_apu->step(cpuCycles, m_gameSpeed);

        // Check for mapper IRQ (for VRC6 and similar mappers that clock IRQ on CPU cycles)
        // This is needed because VRC6 IRQ is clocked during APU step, not at PPU cycle 260
        if (m_cartridge->irqState()) {
            m_cpu->irq();
            m_cartridge->irqClear();
        }
    }

    m_cyclesThisFrame -= CPU_CYCLES_PER_FRAME;
}

bool Core::saveState(const fs::path& filename) {
    Buffer buf = {};
    
    // Save all component states
    buffer_write(&buf, &m_cyclesThisFrame, sizeof(m_cyclesThisFrame));
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
    buffer_read(&buf, &m_cyclesThisFrame, sizeof(m_cyclesThisFrame));
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
