#include "core.h"
#include <SDL3/SDL.h>
#include "../components/buffer.h"

namespace gb {

Core::Core()
    : m_cyclesThisFrame(0) {
}

bool Core::initialize() {
    // Create core components
    m_cartridge = std::make_unique<Cartridge>();
    m_cpu = std::make_unique<CPU>();
    m_mmu = std::make_unique<MMU>();
    m_ppu = std::make_unique<PPU>();
    m_joypad = std::make_unique<Joypad>();
    m_timer = std::make_unique<Timer>();
    m_apu = std::make_unique<APU>();
    m_bootrom = std::make_unique<Bootrom>();

    // Wire up components
    m_mmu->setCartridge(m_cartridge.get());
    m_mmu->setPPU(m_ppu.get());
    m_mmu->setJoypad(m_joypad.get());
    m_mmu->setTimer(m_timer.get());
    m_mmu->setAPU(m_apu.get());
    m_mmu->setBootrom(m_bootrom.get());
    
    m_cpu->setMMU(m_mmu.get());
    m_ppu->setCPU(m_cpu.get());
    m_ppu->setMMU(m_mmu.get());
    m_joypad->setCPU(m_cpu.get());
    m_timer->setCPU(m_cpu.get());
    m_apu->setCPU(m_cpu.get());
    m_apu->setMMU(m_mmu.get());

    return true;
}

void Core::setVideoDevice(VideoDevice* videoDevice) {
    m_ppu->setVideoDevice(videoDevice);
}

void Core::setAudioDevice(AudioDevice* audioDevice) {
    m_apu->setAudioDevice(audioDevice);
}

bool Core::loadBootrom(const fs::path& filename) {
    if (!m_bootrom->load(filename)) {
        log_error("Failed to load bootrom, continuing without it");
        return false;
    }
    return true;
}

bool Core::loadROM(const fs::path& filename) {
    if (!m_cartridge->load(filename)) {
        return false;
    }

    // Enable GBC mode if cartridge supports it
    bool isGBC = m_cartridge->isGBC();
    m_cpu->setGBCMode(isGBC);
    m_ppu->setGBCMode(isGBC);
    m_mmu->setGBCMode(isGBC);

    // Reset bootrom to enabled state if loaded
    if (m_bootrom->isLoaded()) {
        m_bootrom->reset();
    }

    // Reset CPU after loading ROM
    // If bootrom is loaded, start from 0x0000; otherwise skip to 0x0100
    bool useBootrom = m_bootrom->isLoaded();
    m_cpu->reset(useBootrom);
    m_ppu->reset(useBootrom);
    m_timer->reset();
    m_apu->reset();

    return true;
}

void Core::update() {
    // In double speed mode, CPU runs at 2x speed, so we need 2x cycles per frame
    // to maintain the same real-time frame rate
    u32 targetCycles = m_mmu->isDoubleSpeed() ? (CYCLES_PER_FRAME * 2) : CYCLES_PER_FRAME;
    
    // Run until we've executed enough cycles for one frame
    while (m_cyclesThisFrame < targetCycles) {
        u32 cycles = m_cpu->step();
        
        m_ppu->step(cycles);
        m_timer->step(cycles);
        m_apu->step(cycles, m_gameSpeed);
        
        // Add DMA cycles if any DMA occurred
        u32 dmaCycles = m_ppu->getDMACycles();
        if (dmaCycles > 0) {
            m_ppu->clearDMACycles();
            cycles += dmaCycles;
        }
        
        m_cyclesThisFrame += cycles;
    }
    
    m_cyclesThisFrame -= targetCycles;
}

bool Core::saveState(const fs::path& filename) {
    Buffer buf = {};
    
    // Save all component states
    buffer_write(&buf, &m_cyclesThisFrame, sizeof(m_cyclesThisFrame));
    m_cpu->saveState(&buf);
    m_mmu->saveState(&buf);
    m_ppu->saveState(&buf);
    m_timer->saveState(&buf);
    m_joypad->saveState(&buf);
    m_apu->saveState(&buf);
    m_cartridge->saveState(&buf);
    
    return buffer_save_to_file(&buf, filename);
}

bool Core::loadState(const fs::path& filename) {
    Buffer buf = {};
    buffer_load_from_file(&buf, filename);
    
    // Load all component states
    buffer_read(&buf, &m_cyclesThisFrame, sizeof(m_cyclesThisFrame));
    m_cpu->loadState(&buf);
    m_mmu->loadState(&buf);
    m_ppu->loadState(&buf);
    m_timer->loadState(&buf);
    m_joypad->loadState(&buf);
    m_apu->loadState(&buf);
    m_cartridge->loadState(&buf);

    return true;
}

void Core::updateGameSpeed(double gameSpeed) {
    m_gameSpeed = gameSpeed;
}

} // namespace gb
