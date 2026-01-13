#include "core.h"
#include <SDL3/SDL.h>
#include <iostream>
#include <fstream>
#include <sstream>

namespace cps2 {

Core::Core()
    : m_cpuCyclesThisFrame(0)
    , m_soundCpuCyclesThisFrame(0) {
}

bool Core::initialize(VideoDevice* videoDevice, AudioDevice* audioDevice) {
    // Create core components
    m_cartridge = std::make_unique<cps::Cartridge>();
    m_cartridge->setCPSVersion(2);
    m_cpu = std::make_unique<cps::CPU>();
    m_soundCpu = std::make_unique<cps::SoundCPU>();
    m_ppu = std::make_unique<PPU>();
    m_memory = std::make_unique<Memory>();
    m_controller1 = std::make_unique<cps::Controller>();
    m_controller2 = std::make_unique<cps::Controller>();

    // Wire up components
    m_cpu->setMemory(m_memory.get());
    m_soundCpu->setMemory(m_memory.get());
    // Note: Sound CPU APU connection skipped for now (sound system not implemented)
    
    m_ppu->setCPU(m_cpu.get());
    m_ppu->setCartridge(m_cartridge.get());
    m_ppu->setVideoDevice(videoDevice);
    
    m_memory->setCPU(m_cpu.get());
    m_memory->setSoundCPU(m_soundCpu.get());
    m_memory->setPPU(m_ppu.get());
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
    m_soundCpu->reset();
    m_ppu->reset();
    m_memory->reset();
    m_controller1->reset();
    m_controller2->reset();
    m_cartridge->reset();
    
    m_cpuCyclesThisFrame = 0;
    m_soundCpuCyclesThisFrame = 0;

    return true;
}

bool Core::handleInput(SDL_Event& event) {
    switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
            switch (event.key.key) {
                case cps::Config::Key::BUTTON_P1_UP:
                    m_controller1->pressButton(cps::BUTTON_UP);
                    return true;
                case cps::Config::Key::BUTTON_P1_DOWN:
                    m_controller1->pressButton(cps::BUTTON_DOWN);
                    return true;
                case cps::Config::Key::BUTTON_P1_LEFT:
                    m_controller1->pressButton(cps::BUTTON_LEFT);
                    return true;
                case cps::Config::Key::BUTTON_P1_RIGHT:
                    m_controller1->pressButton(cps::BUTTON_RIGHT);
                    return true;
                case cps::Config::Key::BUTTON_P1_PUNCH1:
                    m_controller1->pressButton(cps::BUTTON_PUNCH1);
                    return true;
                case cps::Config::Key::BUTTON_P1_PUNCH2:
                    m_controller1->pressButton(cps::BUTTON_PUNCH2);
                    return true;
                case cps::Config::Key::BUTTON_P1_PUNCH3:
                    m_controller1->pressButton(cps::BUTTON_PUNCH3);
                    return true;
                case cps::Config::Key::BUTTON_P1_KICK1:
                    m_controller1->pressButton(cps::BUTTON_KICK1);
                    return true;
                case cps::Config::Key::BUTTON_P1_KICK2:
                    m_controller1->pressButton(cps::BUTTON_KICK2);
                    return true;
                case cps::Config::Key::BUTTON_P1_KICK3:
                    m_controller1->pressButton(cps::BUTTON_KICK3);
                    return true;
                case cps::Config::Key::BUTTON_P2_UP:
                    m_controller2->pressButton(cps::BUTTON_UP);
                    return true;
                case cps::Config::Key::BUTTON_P2_DOWN:
                    m_controller2->pressButton(cps::BUTTON_DOWN);
                    return true;
                case cps::Config::Key::BUTTON_P2_LEFT:
                    m_controller2->pressButton(cps::BUTTON_LEFT);
                    return true;
                case cps::Config::Key::BUTTON_P2_RIGHT:
                    m_controller2->pressButton(cps::BUTTON_RIGHT);
                    return true;
                case cps::Config::Key::BUTTON_P2_PUNCH1:
                    m_controller2->pressButton(cps::BUTTON_PUNCH1);
                    return true;
                case cps::Config::Key::BUTTON_P2_PUNCH2:
                    m_controller2->pressButton(cps::BUTTON_PUNCH2);
                    return true;
                case cps::Config::Key::BUTTON_P2_PUNCH3:
                    m_controller2->pressButton(cps::BUTTON_PUNCH3);
                    return true;
                case cps::Config::Key::BUTTON_P2_KICK1:
                    m_controller2->pressButton(cps::BUTTON_KICK1);
                    return true;
                case cps::Config::Key::BUTTON_P2_KICK2:
                    m_controller2->pressButton(cps::BUTTON_KICK2);
                    return true;
                case cps::Config::Key::BUTTON_P2_KICK3:
                    m_controller2->pressButton(cps::BUTTON_KICK3);
                    return true;
                case cps::Config::Key::COIN_P1:
                    m_controller1->pressButton(cps::BUTTON_COIN);
                    return true;
                case cps::Config::Key::COIN_P2:
                    m_controller2->pressButton(cps::BUTTON_COIN);
                    return true;
                case cps::Config::Key::START_P1:
                    m_controller1->pressButton(cps::BUTTON_START);
                    return true;
                case cps::Config::Key::START_P2:
                    m_controller2->pressButton(cps::BUTTON_START);
                    return true;
                default:
                    return false;
            }

        case SDL_EVENT_KEY_UP:
            switch (event.key.key) {
                case cps::Config::Key::COIN_P1:
                    m_controller1->releaseButton(cps::BUTTON_COIN);
                    return true;
                case cps::Config::Key::COIN_P2:
                    m_controller2->releaseButton(cps::BUTTON_COIN);
                    return true;
                case cps::Config::Key::BUTTON_P1_UP:
                    m_controller1->releaseButton(cps::BUTTON_UP);
                    return true;
                case cps::Config::Key::BUTTON_P1_DOWN:
                    m_controller1->releaseButton(cps::BUTTON_DOWN);
                    return true;
                case cps::Config::Key::BUTTON_P1_LEFT:
                    m_controller1->releaseButton(cps::BUTTON_LEFT);
                    return true;
                case cps::Config::Key::BUTTON_P1_RIGHT:
                    m_controller1->releaseButton(cps::BUTTON_RIGHT);
                    return true;
                case cps::Config::Key::BUTTON_P1_PUNCH1:
                    m_controller1->releaseButton(cps::BUTTON_PUNCH1);
                    return true;
                case cps::Config::Key::BUTTON_P1_PUNCH2:
                    m_controller1->releaseButton(cps::BUTTON_PUNCH2);
                    return true;
                case cps::Config::Key::BUTTON_P1_PUNCH3:
                    m_controller1->releaseButton(cps::BUTTON_PUNCH3);
                    return true;
                case cps::Config::Key::BUTTON_P1_KICK1:
                    m_controller1->releaseButton(cps::BUTTON_KICK1);
                    return true;
                case cps::Config::Key::BUTTON_P1_KICK2:
                    m_controller1->releaseButton(cps::BUTTON_KICK2);
                    return true;
                case cps::Config::Key::BUTTON_P1_KICK3:
                    m_controller1->releaseButton(cps::BUTTON_KICK3);
                    return true;
                case cps::Config::Key::BUTTON_P2_UP:
                    m_controller2->releaseButton(cps::BUTTON_UP);
                    return true;
                case cps::Config::Key::BUTTON_P2_DOWN:
                    m_controller2->releaseButton(cps::BUTTON_DOWN);
                    return true;
                case cps::Config::Key::BUTTON_P2_LEFT:
                    m_controller2->releaseButton(cps::BUTTON_LEFT);
                    return true;
                case cps::Config::Key::BUTTON_P2_RIGHT:
                    m_controller2->releaseButton(cps::BUTTON_RIGHT);
                    return true;
                case cps::Config::Key::BUTTON_P2_PUNCH1:
                    m_controller2->releaseButton(cps::BUTTON_PUNCH1);
                    return true;
                case cps::Config::Key::BUTTON_P2_PUNCH2:
                    m_controller2->releaseButton(cps::BUTTON_PUNCH2);
                    return true;
                case cps::Config::Key::BUTTON_P2_PUNCH3:
                    m_controller2->releaseButton(cps::BUTTON_PUNCH3);
                    return true;
                case cps::Config::Key::BUTTON_P2_KICK1:
                    m_controller2->releaseButton(cps::BUTTON_KICK1);
                    return true;
                case cps::Config::Key::BUTTON_P2_KICK2:
                    m_controller2->releaseButton(cps::BUTTON_KICK2);
                    return true;
                case cps::Config::Key::BUTTON_P2_KICK3:
                    m_controller2->releaseButton(cps::BUTTON_KICK3);
                    return true;
                case cps::Config::Key::START_P1:
                    m_controller1->releaseButton(cps::BUTTON_START);
                    return true;
                case cps::Config::Key::START_P2:
                    m_controller2->releaseButton(cps::BUTTON_START);
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
        
        // Run sound CPU proportionally (but don't process sound for now)
        u32 soundCpuCycles = static_cast<u32>(cpuCycles * SOUND_CYCLES_RATIO);
        m_soundCpu->step(soundCpuCycles);
        
        // Run PPU (graphics chip runs in parallel)
        // PPU typically runs at similar speed to main CPU
        u32 ppuCycles = cpuCycles;
        for (u32 i = 0; i < ppuCycles; i++) {
            m_ppu->step();
        }
        
        m_cpuCyclesThisFrame += cpuCycles;
        m_soundCpuCyclesThisFrame += soundCpuCycles;
    }
    
    m_ppu->clearFrameComplete();
    m_cpuCyclesThisFrame = 0;
    m_soundCpuCyclesThisFrame = 0;
}

bool Core::saveState(const fs::path& filename) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open save state file: " << filename << std::endl;
        return false;
    }
    
    // Write header
    const char* header = "CPS2EMU";
    file.write(header, 7);
    u32 version = 1;
    file.write(reinterpret_cast<const char*>(&version), sizeof(version));
    
    // Save all component states
    m_cpu->saveState(file);
    m_soundCpu->saveState(file);
    m_ppu->saveState(file);
    m_memory->saveState(file);
    m_cartridge->saveState(file);
    m_controller1->saveState(file);
    m_controller2->saveState(file);
    
    // Save core state
    file.write(reinterpret_cast<const char*>(&m_cpuCyclesThisFrame), sizeof(m_cpuCyclesThisFrame));
    file.write(reinterpret_cast<const char*>(&m_soundCpuCyclesThisFrame), sizeof(m_soundCpuCyclesThisFrame));
    
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
    char header[8] = {0};
    file.read(header, 7);
    if (std::string(header) != "CPS2EMU") {
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
    m_soundCpu->loadState(file);
    m_ppu->loadState(file);
    m_memory->loadState(file);
    m_cartridge->loadState(file);
    m_controller1->loadState(file);
    m_controller2->loadState(file);
    
    // Load core state
    file.read(reinterpret_cast<char*>(&m_cpuCyclesThisFrame), sizeof(m_cpuCyclesThisFrame));
    file.read(reinterpret_cast<char*>(&m_soundCpuCyclesThisFrame), sizeof(m_soundCpuCyclesThisFrame));
    
    file.close();
    return true;
}

void Core::updateGameSpeed(double gameSpeed) {
    m_gameSpeed = gameSpeed;
}

} // namespace cps2
