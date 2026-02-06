#include "core.h"
#include "../components/buffer.h"
#include <SDL3/SDL.h>

namespace cps {

Core::Core()
    : m_cpsVersion(1) {
}

bool Core::initialize() {
    // Create core components
    m_cartridge = std::make_unique<Cartridge>();
    m_cpu = std::make_unique<CPU>();
    m_soundCpu = std::make_unique<SoundCPU>();
    m_ppu = std::make_unique<PPU>();
    m_apu = std::make_unique<APU>();
    m_memory = std::make_unique<Memory>();
    m_controller = std::make_unique<Controller>();

    // Wire up components
    m_cpu->setMemory(m_memory.get());
    m_cpu->setCartridge(m_cartridge.get());
    m_soundCpu->setMemory(m_memory.get());
    m_soundCpu->setAPU(m_apu.get());
    m_soundCpu->setCartridge(m_cartridge.get());

    m_ppu->setCPU(m_cpu.get());
    m_ppu->setCartridge(m_cartridge.get());
    m_ppu->setMemory(m_memory.get());
    
    m_apu->setSoundCPU(m_soundCpu.get());
    m_apu->setMemory(m_memory.get());
    m_apu->setCartridge(m_cartridge.get());
    
    m_memory->setCPU(m_cpu.get());
    m_memory->setSoundCPU(m_soundCpu.get());
    m_memory->setPPU(m_ppu.get());
    m_memory->setAPU(m_apu.get());
    m_memory->setCartridge(m_cartridge.get());
    m_memory->setController(m_controller.get());
    
    m_cartridge->setCPU(m_cpu.get());
    m_cartridge->setPPU(m_ppu.get());

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

    // Set CPS version on components that need it
    m_soundCpu->setCPSVersion(m_cpsVersion);
    m_controller->setCPSVersion(m_cpsVersion);
    
    // Reset all components
    m_cartridge->reset();
    m_cpu->reset();
    m_soundCpu->reset();
    m_ppu->reset();
    m_apu->reset();
    m_memory->reset();
    m_controller->reset();

    return true;
}

void Core::update() {
    // Run until we complete a frame
    // The PPU will set frameComplete when VBlank starts

    u32 nextCpuCycles = CPU_CYCLES_PER_STEP;
    
    while (m_cpu->frameCycles() < m_cpu->cyclesPerFrame()) {
        // Execute CPU cycles)
        u32 cpuCycles = m_cpu->step(nextCpuCycles);
        nextCpuCycles = CPU_CYCLES_PER_STEP;
        
        // Run sound CPU proportionally
        float soundCyclesRatio;
        if (m_cartridge->getCPSVersion() == 2) {
            soundCyclesRatio = ::cps2::SOUND_CYCLES_RATIO;
        } else if (m_cartridge->isCPS1QSound()) {
            soundCyclesRatio = ::cps1qs::SOUND_CYCLES_RATIO;
        } else {
            soundCyclesRatio = ::cps1::SOUND_CYCLES_RATIO;
        }
        s32 soundCpuCycles = static_cast<s32>(m_cpu->frameCycles() * soundCyclesRatio) - m_soundCpu->frameCycles();
        
        // Execute Z80 and get actual cycles executed
        if (soundCpuCycles > 0) {
            u32 soundCpuCyclesActual = m_soundCpu->step(static_cast<u32>(soundCpuCycles));
            m_apu->step(soundCpuCyclesActual, m_gameSpeed);
        }
        
        // Run PPU
        m_ppu->step(cpuCycles);

        // Avoid overrunning the frame by too much
        u32 cyclesLeft = m_cpu->cyclesPerFrame() - m_cpu->frameCycles();
        if (cyclesLeft < nextCpuCycles) {
            nextCpuCycles = cyclesLeft;
        }
    }

    m_cpu->endFrame();
    m_soundCpu->endFrame();
}

bool Core::saveState(const fs::path& filename) {
    Buffer buf = {};
    
    // Save all component states
    m_cpu->saveState(&buf);
    m_soundCpu->saveState(&buf);
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
    m_cpu->loadState(&buf);
    m_soundCpu->loadState(&buf);
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

void Core::setAudioSampleRate(u32 sampleRate) {
    m_apu->setSampleRate(sampleRate);
}

void Core::setAudioVolume(float volume) {
    m_apu->setVolume(volume);
}

} // namespace cps
