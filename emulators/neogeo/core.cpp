#include "core.h"
#include "controller.h"
#include <SDL3/SDL.h>
#include <iostream>
#include <fstream>

namespace neogeo {

Core::Core() {
}

bool Core::initialize(::VideoDevice* videoDevice, ::AudioDevice* audioDevice) {
    // Create core components
    m_cartridge = std::make_unique<Cartridge>();
    m_cpu = std::make_unique<CPU>();
    m_soundCpu = std::make_unique<SoundCPU>();
    m_apu = std::make_unique<APU>();
    m_ppu = std::make_unique<PPU>();
    m_memory = std::make_unique<Memory>();
    m_controller = std::make_unique<Controller>();

    // Wire up components
    m_cpu->setMemory(m_memory.get());
    m_soundCpu->setMemory(m_memory.get());
    m_soundCpu->setAPU(m_apu.get());
    
    m_apu->setSoundCPU(m_soundCpu.get());
    m_apu->setMemory(m_memory.get());
    m_apu->setCartridge(m_cartridge.get());
    m_apu->setAudioDevice(audioDevice);
    
    m_ppu->setCPU(m_cpu.get());
    m_ppu->setCartridge(m_cartridge.get());
    m_ppu->setMemory(m_memory.get());
    m_ppu->setVideoDevice(videoDevice);
    
    m_memory->setCPU(m_cpu.get());
    m_memory->setSoundCPU(m_soundCpu.get());
    m_memory->setPPU(m_ppu.get());
    m_memory->setCartridge(m_cartridge.get());
    m_memory->setController(m_controller.get());
    m_memory->setAPU(m_apu.get());
    
    m_cartridge->setCPU(m_cpu.get());
    m_cartridge->setPPU(m_ppu.get());

    m_controller->setCartridge(m_cartridge.get());

    return true;
}

bool Core::loadROM(const fs::path& filename) {
    if (!m_cartridge->load(filename, m_bios68kIndex)) {
        return false;
    }
    
    // Reset all components
    m_cpu->reset();
    m_soundCpu->reset();
    m_apu->reset();
    m_ppu->reset();
    m_memory->reset();
    m_controller->reset();
    m_cartridge->reset();

    return true;
}

bool Core::handleInput(SDL_Event& event) {
    switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
            switch (event.key.key) {
                case Config::Key::P1_UP:
                    m_controller->pressButton(1, BUTTON_UP);
                    return true;
                case Config::Key::P1_DOWN:
                    m_controller->pressButton(1, BUTTON_DOWN);
                    return true;
                case Config::Key::P1_LEFT:
                    m_controller->pressButton(1, BUTTON_LEFT);
                    return true;
                case Config::Key::P1_RIGHT:
                    m_controller->pressButton(1, BUTTON_RIGHT);
                    return true;
                case Config::Key::P1_BUTTON_A:
                    m_controller->pressButton(1, BUTTON_A);
                    return true;
                case Config::Key::P1_BUTTON_B:
                    m_controller->pressButton(1, BUTTON_B);
                    return true;
                case Config::Key::P1_BUTTON_C:
                    m_controller->pressButton(1, BUTTON_C);
                    return true;
                case Config::Key::P1_BUTTON_D:
                    m_controller->pressButton(1, BUTTON_D);
                    return true;
                case Config::Key::P2_UP:
                    m_controller->pressButton(2, BUTTON_UP);
                    return true;
                case Config::Key::P2_DOWN:
                    m_controller->pressButton(2, BUTTON_DOWN);
                    return true;
                case Config::Key::P2_LEFT:
                    m_controller->pressButton(2, BUTTON_LEFT);
                    return true;
                case Config::Key::P2_RIGHT:
                    m_controller->pressButton(2, BUTTON_RIGHT);
                    return true;
                case Config::Key::P2_BUTTON_A:
                    m_controller->pressButton(2, BUTTON_A);
                    return true;
                case Config::Key::P2_BUTTON_B:
                    m_controller->pressButton(2, BUTTON_B);
                    return true;
                case Config::Key::P2_BUTTON_C:
                    m_controller->pressButton(2, BUTTON_C);
                    return true;
                case Config::Key::P2_BUTTON_D:
                    m_controller->pressButton(2, BUTTON_D);
                    return true;
                case Config::Key::P1_COIN:
                    m_controller->pressButton(1, BUTTON_COIN);
                    return true;
                case Config::Key::P2_COIN:
                    m_controller->pressButton(2, BUTTON_COIN);
                    return true;
                case Config::Key::P1_START:
                    m_controller->pressButton(1, BUTTON_START);
                    return true;
                case Config::Key::P2_START:
                    m_controller->pressButton(2, BUTTON_START);
                    return true;
                case Config::Key::P1_SELECT:
                    m_controller->pressButton(1, BUTTON_SELECT);
                    return true;
                case Config::Key::P2_SELECT:
                    m_controller->pressButton(2, BUTTON_SELECT);
                    return true;
                case Config::Key::TEST:
                    m_controller->pressButton(0, BUTTON_TEST);
                    return true;
                case Config::Key::SERVICE:
                    m_controller->pressButton(0, BUTTON_SERVICE);
                    return true;
                default:
                    return false;
            }

        case SDL_EVENT_KEY_UP:
            switch (event.key.key) {
                case Config::Key::P1_UP:
                    m_controller->releaseButton(1, BUTTON_UP);
                    return true;
                case Config::Key::P1_DOWN:
                    m_controller->releaseButton(1, BUTTON_DOWN);
                    return true;
                case Config::Key::P1_LEFT:
                    m_controller->releaseButton(1, BUTTON_LEFT);
                    return true;
                case Config::Key::P1_RIGHT:
                    m_controller->releaseButton(1, BUTTON_RIGHT);
                    return true;
                case Config::Key::P1_BUTTON_A:
                    m_controller->releaseButton(1, BUTTON_A);
                    return true;
                case Config::Key::P1_BUTTON_B:
                    m_controller->releaseButton(1, BUTTON_B);
                    return true;
                case Config::Key::P1_BUTTON_C:
                    m_controller->releaseButton(1, BUTTON_C);
                    return true;
                case Config::Key::P1_BUTTON_D:
                    m_controller->releaseButton(1, BUTTON_D);
                    return true;
                case Config::Key::P2_UP:
                    m_controller->releaseButton(2, BUTTON_UP);
                    return true;
                case Config::Key::P2_DOWN:
                    m_controller->releaseButton(2, BUTTON_DOWN);
                    return true;
                case Config::Key::P2_LEFT:
                    m_controller->releaseButton(2, BUTTON_LEFT);
                    return true;
                case Config::Key::P2_RIGHT:
                    m_controller->releaseButton(2, BUTTON_RIGHT);
                    return true;
                case Config::Key::P2_BUTTON_A:
                    m_controller->releaseButton(2, BUTTON_A);
                    return true;
                case Config::Key::P2_BUTTON_B:
                    m_controller->releaseButton(2, BUTTON_B);
                    return true;
                case Config::Key::P2_BUTTON_C:
                    m_controller->releaseButton(2, BUTTON_C);
                    return true;
                case Config::Key::P2_BUTTON_D:
                    m_controller->releaseButton(2, BUTTON_D);
                    return true;
                case Config::Key::P1_COIN:
                    m_controller->releaseButton(1, BUTTON_COIN);
                    return true;
                case Config::Key::P2_COIN:
                    m_controller->releaseButton(2, BUTTON_COIN);
                    return true;
                case Config::Key::P1_START:
                    m_controller->releaseButton(1, BUTTON_START);
                    return true;
                case Config::Key::P2_START:
                    m_controller->releaseButton(2, BUTTON_START);
                    return true;
                case Config::Key::P1_SELECT:
                    m_controller->releaseButton(1, BUTTON_SELECT);
                    return true;
                case Config::Key::P2_SELECT:
                    m_controller->releaseButton(2, BUTTON_SELECT);
                    return true;
                case Config::Key::TEST:
                    m_controller->releaseButton(0, BUTTON_TEST);
                    return true;
                case Config::Key::SERVICE:
                    m_controller->releaseButton(0, BUTTON_SERVICE);
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
    
    // Track accumulated excess/deficit for Z80 synchronization
    s32 soundCpuSyncOffset = 0;
    
    while (!m_ppu->isFrameComplete()) {
        // Track cycles before instruction
        u32 cyclesBefore = m_cpu->getCycles();
        
        // Execute one CPU instruction (takes multiple cycles)
        m_cpu->step();
        
        // Calculate how many CPU cycles the instruction took
        u32 cpuCycles = m_cpu->getCycles() - cyclesBefore;
        
        // Run sound CPU proportionally
        s32 soundCpuCycles = static_cast<u32>(cpuCycles * SOUND_CYCLES_RATIO) - soundCpuSyncOffset;
        
        // Execute Z80 and get actual cycles executed
        u32 soundCpuCyclesActual = m_soundCpu->step(static_cast<u32>(soundCpuCycles));
        soundCpuSyncOffset = static_cast<s32>(soundCpuCyclesActual) - static_cast<s32>(soundCpuCycles);

        // Run APU using actual cycles executed
        // m_apu->step(soundCpuCyclesActual, m_gameSpeed);
        
        // Run PPU (graphics chip runs in parallel)
        // PPU typically runs at similar speed to main CPU
        u32 ppuCycles = cpuCycles;
        for (u32 i = 0; i < ppuCycles; i++) {
            m_ppu->step();
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
    const char* header = "NEOEMU";
    file.write(header, 6);
    
    // Save all component states
    m_cpu->saveState(file);
    m_soundCpu->saveState(file);
    m_apu->saveState(file);
    m_ppu->saveState(file);
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
    if (std::string(header) != "NEOEMU") {
        std::cerr << "Invalid save state file format" << std::endl;
        return false;
    }
    
    // Load all component states
    m_cpu->loadState(file);
    m_soundCpu->loadState(file);
    m_apu->loadState(file);
    m_ppu->loadState(file);
    m_memory->loadState(file);
    m_cartridge->loadState(file);
    m_controller->loadState(file);
    
    file.close();
    return true;
}

void Core::updateGameSpeed(double gameSpeed) {
    m_gameSpeed = gameSpeed;
}

void Core::setAudioSampleRate(u32 sampleRate) {
    if (m_apu) {
        m_apu->setSampleRate(sampleRate);
    }
}

void Core::setAudioVolume(float volume) {
    if (m_apu) {
        m_apu->setVolume(volume);
    }
}

} // namespace neogeo
