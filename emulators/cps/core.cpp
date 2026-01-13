#include "core.h"
#include <SDL3/SDL.h>
#include <iostream>
#include <fstream>
#include <sstream>

namespace cps {

Core::Core()
    : m_cpuCyclesThisFrame(0)
    , m_soundCpuCyclesThisFrame(0)
    , m_cpsVersion(1) {
}

bool Core::initialize(::VideoDevice* videoDevice, ::AudioDevice* audioDevice) {
    // Create core components
    m_cartridge = std::make_unique<Cartridge>();
    m_cpu = std::make_unique<CPU>();
    m_soundCpu = std::make_unique<SoundCPU>();
    m_ppu = std::make_unique<PPU>();
    m_apu = std::make_unique<::cps1::APU>();  // Use CPS1 APU for both
    m_memory = std::make_unique<Memory>();
    m_controller1 = std::make_unique<Controller>();
    m_controller2 = std::make_unique<Controller>();

    // Wire up components
    m_cpu->setMemory(m_memory.get());
    m_soundCpu->setMemory(m_memory.get());
    m_soundCpu->setAPU(m_apu.get());
    
    m_ppu->setCPU(m_cpu.get());
    m_ppu->setCartridge(m_cartridge.get());
    m_ppu->setVideoDevice(videoDevice);
    
    m_apu->setSoundCPU(m_soundCpu.get());
    m_apu->setMemory(m_memory.get());
    m_apu->setCartridge(m_cartridge.get());
    m_apu->setAudioDevice(audioDevice);
    
    m_memory->setCPU(m_cpu.get());
    m_memory->setSoundCPU(m_soundCpu.get());
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
    
    // Get CPS version from cartridge
    m_cpsVersion = m_cartridge->getCPSVersion();
    
    // Set CPS version in cartridge if not already set
    if (m_cpsVersion == 0) {
        // Determine from game info
        const GameInfo* gameInfo = m_cartridge->getGameInfo();
        if (gameInfo) {
            m_cpsVersion = gameInfo->cpsVer;
            m_cartridge->setCPSVersion(m_cpsVersion);
        }
    }

    // Reset all components
    m_cpu->reset();
    m_soundCpu->reset();
    m_ppu->reset();
    m_apu->reset();
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
                case Config::Key::BUTTON_P1_UP:
                    m_controller1->pressButton(BUTTON_UP);
                    return true;
                case Config::Key::BUTTON_P1_DOWN:
                    m_controller1->pressButton(BUTTON_DOWN);
                    return true;
                case Config::Key::BUTTON_P1_LEFT:
                    m_controller1->pressButton(BUTTON_LEFT);
                    return true;
                case Config::Key::BUTTON_P1_RIGHT:
                    m_controller1->pressButton(BUTTON_RIGHT);
                    return true;
                case Config::Key::BUTTON_P1_PUNCH1:
                    m_controller1->pressButton(BUTTON_PUNCH1);
                    return true;
                case Config::Key::BUTTON_P1_PUNCH2:
                    m_controller1->pressButton(BUTTON_PUNCH2);
                    return true;
                case Config::Key::BUTTON_P1_PUNCH3:
                    m_controller1->pressButton(BUTTON_PUNCH3);
                    return true;
                case Config::Key::BUTTON_P1_KICK1:
                    m_controller1->pressButton(BUTTON_KICK1);
                    return true;
                case Config::Key::BUTTON_P1_KICK2:
                    m_controller1->pressButton(BUTTON_KICK2);
                    return true;
                case Config::Key::BUTTON_P1_KICK3:
                    m_controller1->pressButton(BUTTON_KICK3);
                    return true;
                case Config::Key::BUTTON_P2_UP:
                    m_controller2->pressButton(BUTTON_UP);
                    return true;
                case Config::Key::BUTTON_P2_DOWN:
                    m_controller2->pressButton(BUTTON_DOWN);
                    return true;
                case Config::Key::BUTTON_P2_LEFT:
                    m_controller2->pressButton(BUTTON_LEFT);
                    return true;
                case Config::Key::BUTTON_P2_RIGHT:
                    m_controller2->pressButton(BUTTON_RIGHT);
                    return true;
                case Config::Key::BUTTON_P2_PUNCH1:
                    m_controller2->pressButton(BUTTON_PUNCH1);
                    return true;
                case Config::Key::BUTTON_P2_PUNCH2:
                    m_controller2->pressButton(BUTTON_PUNCH2);
                    return true;
                case Config::Key::BUTTON_P2_PUNCH3:
                    m_controller2->pressButton(BUTTON_PUNCH3);
                    return true;
                case Config::Key::BUTTON_P2_KICK1:
                    m_controller2->pressButton(BUTTON_KICK1);
                    return true;
                case Config::Key::BUTTON_P2_KICK2:
                    m_controller2->pressButton(BUTTON_KICK2);
                    return true;
                case Config::Key::BUTTON_P2_KICK3:
                    m_controller2->pressButton(BUTTON_KICK3);
                    return true;
                case Config::Key::COIN_P1:
                    m_controller1->pressButton(BUTTON_COIN);
                    return true;
                case Config::Key::COIN_P2:
                    m_controller2->pressButton(BUTTON_COIN);
                    return true;
                case Config::Key::START_P1:
                    m_controller1->pressButton(BUTTON_START);
                    return true;
                case Config::Key::START_P2:
                    m_controller2->pressButton(BUTTON_START);
                    return true;
                default:
                    return false;
            }

        case SDL_EVENT_KEY_UP:
            switch (event.key.key) {
                case Config::Key::COIN_P1:
                    m_controller1->releaseButton(BUTTON_COIN);
                    return true;
                case Config::Key::COIN_P2:
                    m_controller2->releaseButton(BUTTON_COIN);
                    return true;
                case Config::Key::BUTTON_P1_UP:
                    m_controller1->releaseButton(BUTTON_UP);
                    return true;
                case Config::Key::BUTTON_P1_DOWN:
                    m_controller1->releaseButton(BUTTON_DOWN);
                    return true;
                case Config::Key::BUTTON_P1_LEFT:
                    m_controller1->releaseButton(BUTTON_LEFT);
                    return true;
                case Config::Key::BUTTON_P1_RIGHT:
                    m_controller1->releaseButton(BUTTON_RIGHT);
                    return true;
                case Config::Key::BUTTON_P1_PUNCH1:
                    m_controller1->releaseButton(BUTTON_PUNCH1);
                    return true;
                case Config::Key::BUTTON_P1_PUNCH2:
                    m_controller1->releaseButton(BUTTON_PUNCH2);
                    return true;
                case Config::Key::BUTTON_P1_PUNCH3:
                    m_controller1->releaseButton(BUTTON_PUNCH3);
                    return true;
                case Config::Key::BUTTON_P1_KICK1:
                    m_controller1->releaseButton(BUTTON_KICK1);
                    return true;
                case Config::Key::BUTTON_P1_KICK2:
                    m_controller1->releaseButton(BUTTON_KICK2);
                    return true;
                case Config::Key::BUTTON_P1_KICK3:
                    m_controller1->releaseButton(BUTTON_KICK3);
                    return true;
                case Config::Key::BUTTON_P2_UP:
                    m_controller2->releaseButton(BUTTON_UP);
                    return true;
                case Config::Key::BUTTON_P2_DOWN:
                    m_controller2->releaseButton(BUTTON_DOWN);
                    return true;
                case Config::Key::BUTTON_P2_LEFT:
                    m_controller2->releaseButton(BUTTON_LEFT);
                    return true;
                case Config::Key::BUTTON_P2_RIGHT:
                    m_controller2->releaseButton(BUTTON_RIGHT);
                    return true;
                case Config::Key::BUTTON_P2_PUNCH1:
                    m_controller2->releaseButton(BUTTON_PUNCH1);
                    return true;
                case Config::Key::BUTTON_P2_PUNCH2:
                    m_controller2->releaseButton(BUTTON_PUNCH2);
                    return true;
                case Config::Key::BUTTON_P2_PUNCH3:
                    m_controller2->releaseButton(BUTTON_PUNCH3);
                    return true;
                case Config::Key::BUTTON_P2_KICK1:
                    m_controller2->releaseButton(BUTTON_KICK1);
                    return true;
                case Config::Key::BUTTON_P2_KICK2:
                    m_controller2->releaseButton(BUTTON_KICK2);
                    return true;
                case Config::Key::BUTTON_P2_KICK3:
                    m_controller2->releaseButton(BUTTON_KICK3);
                    return true;
                case Config::Key::START_P1:
                    m_controller1->releaseButton(BUTTON_START);
                    return true;
                case Config::Key::START_P2:
                    m_controller2->releaseButton(BUTTON_START);
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
        
        // Run sound CPU proportionally
        float soundCyclesRatio = m_cartridge->getCPSVersion() == 1 ? ::cps1::SOUND_CYCLES_RATIO : ::cps2::SOUND_CYCLES_RATIO;
        u32 soundCpuCycles = static_cast<u32>(cpuCycles * soundCyclesRatio);
        m_soundCpu->step(soundCpuCycles);
        
        // Run PPU (graphics chip runs in parallel)
        // PPU typically runs at similar speed to main CPU
        u32 ppuCycles = cpuCycles;
        for (u32 i = 0; i < ppuCycles; i++) {
            m_ppu->step();
        }
        
        // Run APU
        m_apu->step(soundCpuCycles, m_gameSpeed);
        
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
    
    // Write header (unified for both CPS1 and CPS2)
    const char* header = "CPSEMU";
    file.write(header, 6);
    
    // Save all component states
    m_cpu->saveState(file);
    m_soundCpu->saveState(file);
    m_ppu->saveState(file);
    m_apu->saveState(file);
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
    
    // Read and verify header (support both old and new formats)
    char header[8] = {0};
    file.read(header, 7);
    
    file.seekg(0);
    file.read(header, 6);
    if (std::string(header) != "CPSEMU") {
        std::cerr << "Invalid save state file format" << std::endl;
        return false;
    }
    
    // Load all component states
    m_cpu->loadState(file);
    m_soundCpu->loadState(file);
    m_ppu->loadState(file);
    m_apu->loadState(file);
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

void Core::setAudioSampleRate(u32 sampleRate) {
    m_apu->setSampleRate(sampleRate);
}

void Core::setAudioVolume(float volume) {
    m_apu->setVolume(volume);
}

} // namespace cps
