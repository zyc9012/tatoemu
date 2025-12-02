#include "core.h"
#include <SDL3/SDL.h>
#include <iostream>
#include <fstream>
#include <sstream>

namespace nes {

Core::Core()
    : m_cpuCyclesThisFrame(0)
    , m_ppuCyclesThisFrame(0) {
}

bool Core::initialize(VideoDevice* videoDevice, AudioDevice* audioDevice) {
    // Create core components
    m_cartridge = std::make_unique<Cartridge>();
    m_cpu = std::make_unique<CPU>();
    m_ppu = std::make_unique<PPU>();
    m_apu = std::make_unique<APU>();
    m_memory = std::make_unique<Memory>();
    m_controller1 = std::make_unique<Controller>();
    m_controller2 = std::make_unique<Controller>();

    // Wire up components
    m_cpu->setMemory(m_memory.get());
    
    m_ppu->setCPU(m_cpu.get());
    m_ppu->setCartridge(m_cartridge.get());
    m_ppu->setVideoDevice(videoDevice);
    
    m_apu->setCPU(m_cpu.get());
    m_apu->setMemory(m_memory.get());
    m_apu->setCartridge(m_cartridge.get());
    m_apu->setAudioDevice(audioDevice);
    
    m_memory->setCPU(m_cpu.get());
    m_memory->setPPU(m_ppu.get());
    m_memory->setAPU(m_apu.get());
    m_memory->setCartridge(m_cartridge.get());
    m_memory->setController1(m_controller1.get());
    m_memory->setController2(m_controller2.get());
    
    m_cartridge->setCPU(m_cpu.get());
    m_cartridge->setPPU(m_ppu.get());

    return true;
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
    m_controller1->reset();
    m_controller2->reset();
    m_cartridge->reset();
    
    m_cpuCyclesThisFrame = 0;
    m_ppuCyclesThisFrame = 0;

    return true;
}

bool Core::handleInput(SDL_Event& event) {
    switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
            switch (event.key.key) {
                case Config::Key::BUTTON_A:
                    m_controller1->pressButton(BUTTON_A);
                    return true;
                case Config::Key::BUTTON_B:
                    m_controller1->pressButton(BUTTON_B);
                    return true;
                case Config::Key::START:
                    m_controller1->pressButton(BUTTON_START);
                    return true;
                case Config::Key::SELECT_PRIMARY:
                case Config::Key::SELECT_SECONDARY:
                    m_controller1->pressButton(BUTTON_SELECT);
                    return true;
                case Config::Key::DPAD_UP:
                    m_controller1->pressButton(BUTTON_UP);
                    return true;
                case Config::Key::DPAD_DOWN:
                    m_controller1->pressButton(BUTTON_DOWN);
                    return true;
                case Config::Key::DPAD_LEFT:
                    m_controller1->pressButton(BUTTON_LEFT);
                    return true;
                case Config::Key::DPAD_RIGHT:
                    m_controller1->pressButton(BUTTON_RIGHT);
                    return true;
                default:
                    return false;
            }

        case SDL_EVENT_KEY_UP:
            switch (event.key.key) {
                case Config::Key::BUTTON_A:
                    m_controller1->releaseButton(BUTTON_A);
                    return true;
                case Config::Key::BUTTON_B:
                    m_controller1->releaseButton(BUTTON_B);
                    return true;
                case Config::Key::START:
                    m_controller1->releaseButton(BUTTON_START);
                    return true;
                case Config::Key::SELECT_PRIMARY:
                case Config::Key::SELECT_SECONDARY:
                    m_controller1->releaseButton(BUTTON_SELECT);
                    return true;
                case Config::Key::DPAD_UP:
                    m_controller1->releaseButton(BUTTON_UP);
                    return true;
                case Config::Key::DPAD_DOWN:
                    m_controller1->releaseButton(BUTTON_DOWN);
                    return true;
                case Config::Key::DPAD_LEFT:
                    m_controller1->releaseButton(BUTTON_LEFT);
                    return true;
                case Config::Key::DPAD_RIGHT:
                    m_controller1->releaseButton(BUTTON_RIGHT);
                    return true;
                default:
                    return false;
            }
    }
    return false;
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
            
            // Check for scanline counter (for MMC3 IRQ)
            // MMC3 clocks on A12 rising edge, which happens during PPU rendering
            if (m_ppu->getCycle() == 260 && m_ppu->getScanline() < 240) {
                m_cartridge->scanlineCounter();
                if (m_cartridge->irqState()) {
                    m_cpu->irq();
                    m_cartridge->irqClear();
                }
            }

            // Break out of the loop if the frame is complete
            if (m_ppu->isFrameComplete()) {
                break;
            }
        }
        
        // Run APU
        m_apu->step(cpuCycles, m_gameSpeed);
        
        m_cpuCyclesThisFrame += cpuCycles;
    }
    
    m_ppu->clearFrameComplete();
    m_cpuCyclesThisFrame = 0;
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
    m_controller1->saveState(file);
    m_controller2->saveState(file);
    
    // Save core state
    file.write(reinterpret_cast<const char*>(&m_cpuCyclesThisFrame), sizeof(m_cpuCyclesThisFrame));
    file.write(reinterpret_cast<const char*>(&m_ppuCyclesThisFrame), sizeof(m_ppuCyclesThisFrame));
    
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
    m_controller1->loadState(file);
    m_controller2->loadState(file);
    
    // Load core state
    file.read(reinterpret_cast<char*>(&m_cpuCyclesThisFrame), sizeof(m_cpuCyclesThisFrame));
    file.read(reinterpret_cast<char*>(&m_ppuCyclesThisFrame), sizeof(m_ppuCyclesThisFrame));
    
    file.close();
    return true;
}

void Core::updateGameSpeed(double gameSpeed) {
    m_gameSpeed = gameSpeed;
}

} // namespace nes
