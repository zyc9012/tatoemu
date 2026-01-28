#include "core.h"
#include "controller.h"
#include <SDL3/SDL.h>
#include <iostream>
#include <fstream>

namespace neogeo {

Core::Core() {
}

bool Core::initialize() {
    // Create core components
    m_cartridge = std::make_unique<Cartridge>();
    m_cpu = std::make_unique<CPU>();
    m_soundCpu = std::make_unique<SoundCPU>();
    m_apu = std::make_unique<APU>();
    m_ppu = std::make_unique<PPU>();
    m_memory = std::make_unique<Memory>();
    m_controller = std::make_unique<Controller>();
    m_upd4990a = std::make_unique<UPD4990A>();

    // Wire up components
    m_cpu->setMemory(m_memory.get());
    m_soundCpu->setMemory(m_memory.get());
    m_soundCpu->setAPU(m_apu.get());
    
    m_apu->setSoundCPU(m_soundCpu.get());
    m_apu->setMemory(m_memory.get());
    m_apu->setCartridge(m_cartridge.get());
    
    m_ppu->setCPU(m_cpu.get());
    m_ppu->setCartridge(m_cartridge.get());
    m_ppu->setMemory(m_memory.get());
    
    m_memory->setCPU(m_cpu.get());
    m_memory->setSoundCPU(m_soundCpu.get());
    m_memory->setPPU(m_ppu.get());
    m_memory->setCartridge(m_cartridge.get());
    m_memory->setController(m_controller.get());
    m_memory->setAPU(m_apu.get());
    m_memory->setUPD4990A(m_upd4990a.get());
    m_memory->setCore(this);
    
    m_cartridge->setCPU(m_cpu.get());
    m_cartridge->setPPU(m_ppu.get());

    m_controller->setCartridge(m_cartridge.get());

    return true;
}

void Core::setVideoDevice(::VideoDevice* videoDevice) {
    if (m_ppu) {
        m_ppu->setVideoDevice(videoDevice);
    }
}

void Core::setAudioDevice(::AudioDevice* audioDevice) {
    if (m_apu) {
        m_apu->setAudioDevice(audioDevice);
    }
}

bool Core::loadROM(const fs::path& filename) {
    if (!m_cartridge->load(filename, Config::BiosIndex)) {
        return false;
    }
    
    reset();

    return true;
}

void Core::reset() {
    m_cpu->reset();
    m_soundCpu->reset();
    m_apu->reset();
    m_ppu->reset();
    m_memory->reset();
    m_controller->reset();
    m_cartridge->reset();
    m_upd4990a->initialize(CPU_FREQUENCY, [this]() { return m_cpu->getCycles(); });

    m_watchdogTimer = 0;
}

void Core::update() {
    // Run until we complete a frame
    // The PPU will set frameComplete when VBlank starts
    
    // Track accumulated excess/deficit for Z80 synchronization
    s32 soundCpuSyncOffset = 0;
    u32 cyclesThisFrame = 0;
    
    while (!m_ppu->isFrameComplete()) {
        // Execute CPU cycles
        u32 cpuCycles = m_cpu->step(50);

        // Update cycle counter
        cyclesThisFrame += cpuCycles;

        // Run sound CPU proportionally
        s32 soundCpuCycles = static_cast<u32>(cpuCycles * SOUND_CYCLES_RATIO) - soundCpuSyncOffset;
        
        // Execute Z80 and get actual cycles executed
        u32 soundCpuCyclesActual = m_soundCpu->step(static_cast<u32>(soundCpuCycles));
        soundCpuSyncOffset = static_cast<s32>(soundCpuCyclesActual) - static_cast<s32>(soundCpuCycles);

        // Run APU using actual cycles executed
        m_apu->step(soundCpuCyclesActual, m_gameSpeed);
        
        // Run PPU (graphics chip runs in parallel)
        // PPU typically runs at similar speed to main CPU
        u32 ppuCycles = cpuCycles;
        for (u32 i = 0; i < ppuCycles; i++) {
            m_ppu->step();
        }
    }
    
    m_ppu->clearFrameComplete();

    // Update watchdog timer
    if (m_cartridge) {
        m_watchdogTimer += cyclesThisFrame;

        // Check if watchdog has expired (should trigger every ~8 frames)
        if (m_watchdogTimer > static_cast<s32>(WATCHDOG_TIMEOUT_CYCLES)) {
            std::cout << "Watchdog timer expired, resetting system..." << std::endl;
            reset();
            return;
        }
    }
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
    m_upd4990a->saveState(file);
    
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
    m_upd4990a->loadState(file);
    
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
