#include "core.h"
#include <SDL3/SDL.h>
#include <iostream>
#include <fstream>
#include <sstream>

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
    if (m_ppu) {
        m_ppu->setVideoDevice(videoDevice);
    }
}

void Core::setAudioDevice(AudioDevice* audioDevice) {
    if (m_apu) {
        m_apu->setAudioDevice(audioDevice);
    }
}

bool Core::loadBootrom(const fs::path& filename) {
    if (!m_bootrom->load(filename)) {
        std::cerr << "Failed to load bootrom, continuing without it" << std::endl;
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

bool Core::handleInput(SDL_Event& event) {
    switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
            switch (event.key.key) {
                case Config::Key::BUTTON_A: // A button
                    m_joypad->pressButton(BUTTON_A);
                    return true;
                case Config::Key::BUTTON_B: // B button
                    m_joypad->pressButton(BUTTON_B);
                    return true;
                case Config::Key::START: // Start
                    m_joypad->pressButton(BUTTON_START);
                    return true;
                case Config::Key::SELECT_PRIMARY: // Select
                case Config::Key::SELECT_SECONDARY:
                    m_joypad->pressButton(BUTTON_SELECT);
                    return true;
                case Config::Key::DPAD_UP:
                    m_joypad->pressButton(BUTTON_UP);
                    return true;
                case Config::Key::DPAD_DOWN:
                    m_joypad->pressButton(BUTTON_DOWN);
                    return true;
                case Config::Key::DPAD_LEFT:
                    m_joypad->pressButton(BUTTON_LEFT);
                    return true;
                case Config::Key::DPAD_RIGHT:
                    m_joypad->pressButton(BUTTON_RIGHT);
                    return true;
                default:
                    return false;
            }

        case SDL_EVENT_KEY_UP:
            switch (event.key.key) {
                case Config::Key::BUTTON_A: // A button
                    m_joypad->releaseButton(BUTTON_A);
                    return true;
                case Config::Key::BUTTON_B: // B button
                    m_joypad->releaseButton(BUTTON_B);
                    return true;
                case Config::Key::START: // Start
                    m_joypad->releaseButton(BUTTON_START);
                    return true;
                case Config::Key::SELECT_PRIMARY: // Select
                case Config::Key::SELECT_SECONDARY:
                    m_joypad->releaseButton(BUTTON_SELECT);
                    return true;
                case Config::Key::DPAD_UP:
                    m_joypad->releaseButton(BUTTON_UP);
                    return true;
                case Config::Key::DPAD_DOWN:
                    m_joypad->releaseButton(BUTTON_DOWN);
                    return true;
                case Config::Key::DPAD_LEFT:
                    m_joypad->releaseButton(BUTTON_LEFT);
                    return true;
                case Config::Key::DPAD_RIGHT:
                    m_joypad->releaseButton(BUTTON_RIGHT);
                    return true;
                default:
                    return false;
            }
    }
    return false;
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
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open save state file: " << filename << std::endl;
        return false;
    }
    
    // Write a simple header
    const char* header = "GBEMU";
    file.write(header, 5);
    u32 version = 1;
    file.write(reinterpret_cast<const char*>(&version), sizeof(version));
    
    // Save all component states
    m_cpu->saveState(file);
    m_mmu->saveState(file);
    m_ppu->saveState(file);
    m_timer->saveState(file);
    m_joypad->saveState(file);
    m_apu->saveState(file);
    m_cartridge->saveState(file);
    
    // Save emulator state
    file.write(reinterpret_cast<const char*>(&m_cyclesThisFrame), sizeof(m_cyclesThisFrame));
    
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
    char header[6] = {0};
    file.read(header, 5);
    if (std::string(header) != "GBEMU") {
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
    m_mmu->loadState(file);
    m_ppu->loadState(file);
    m_timer->loadState(file);
    m_joypad->loadState(file);
    m_apu->loadState(file);
    m_cartridge->loadState(file);

    // Load emulator state
    file.read(reinterpret_cast<char*>(&m_cyclesThisFrame), sizeof(m_cyclesThisFrame));
    
    file.close();
    return true;
}

void Core::updateGameSpeed(double gameSpeed) {
    m_gameSpeed = gameSpeed;
}

} // namespace gb
