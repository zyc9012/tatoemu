#include "core.h"
#include <SDL3/SDL.h>
#include <iostream>
#include <fstream>
#include <sstream>

namespace nes {

Core::Core() {
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
    // Run until we complete a frame
    // The PPU will set frameComplete when VBlank starts
    
    while (!m_ppu->isFrameComplete()) {
        // Track cycles before instruction
        u32 cyclesBefore = m_cpu->getCycles();
        
        // Execute one CPU instruction (takes multiple cycles)
        m_cpu->step();
        
        // Calculate how many CPU cycles the instruction took
        u32 cpuCycles = m_cpu->getCycles() - cyclesBefore;
        
        // Run PPU for 3 PPU cycles per CPU cycle
        u32 ppuCycles = cpuCycles * PPU_CYCLES_PER_CPU;
        for (u32 i = 0; i < ppuCycles; i++) {
            m_ppu->step();

            // Break out of the loop if the frame is complete
            if (m_ppu->isFrameComplete()) {
                break;
            }
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
    
    m_ppu->clearFrameComplete();
}

bool Core::saveState(const fs::path& filename) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open save state file: " << filename << std::endl;
        return false;
    }
    
    // Write header
    const char* header = "NESEMU";
    file.write(header, 6);
    u32 version = 1;
    file.write(reinterpret_cast<const char*>(&version), sizeof(version));
    
    // Save all component states
    m_cpu->saveState(file);
    m_ppu->saveState(file);
    m_apu->saveState(file);
    m_memory->saveState(file);
    m_cartridge->saveState(file);
    m_controller->saveState(file);
    
    file.close();
    return true;
}

bool Core::loadState(const fs::path& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open save state file: " << filename << std::endl;
        return false;
    }
    
    // Read and verify header
    char header[7] = {0};
    file.read(header, 6);
    if (std::string(header) != "NESEMU") {
        std::cerr << "Invalid save state file format" << std::endl;
        return false;
    }
    
    u32 version;
    file.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (version != 1) {
        std::cerr << "Unsupported save state version" << std::endl;
        return false;
    }
    
    // Load all component states
    m_cpu->loadState(file);
    m_ppu->loadState(file);
    m_apu->loadState(file);
    m_memory->loadState(file);
    m_cartridge->loadState(file);
    m_controller->loadState(file);
    
    file.close();
    return true;
}

void Core::updateGameSpeed(double gameSpeed) {
    m_gameSpeed = gameSpeed;
}

} // namespace nes
