#include "core.h"
#include "controller.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include "../../components/buffer.h"

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
    m_cartridge->reset();
    m_cpu->reset();
    m_soundCpu->reset();
    m_apu->reset();
    m_ppu->reset();
    m_memory->reset();
    m_controller->reset();
    m_upd4990a->initialize(CPU_FREQUENCY, [this]() { return m_cpu->getCycles(); });

    m_watchdogTimer = 0;
}

void Core::update() {
    // Run until we complete a frame
    // The PPU will set frameComplete when VBlank starts
    
    u32 cyclesThisFrame = 0;
    u32 nextCpuCycles = CPU_CYCLES_PER_STEP;
    
    while (!m_ppu->isFrameComplete()) {
        // Execute CPU cycles
        u32 oldCycles = m_cpu->getCycles();
        u32 cpuCycles = m_cpu->step(nextCpuCycles);
        nextCpuCycles = CPU_CYCLES_PER_STEP;

        // Update cycle counter
        cyclesThisFrame += cpuCycles;

        // Run sound CPU proportionally
        s32 soundCpuCycles = m_cpu->getCycles() * SOUND_CYCLES_RATIO - m_soundCpu->getCycles();
        
        // Execute Z80 and get actual cycles executed
        if (soundCpuCycles > 0) {
            u32 soundCpuCyclesActual = m_soundCpu->step(static_cast<u32>(soundCpuCycles));

            // Run APU using actual cycles executed
            m_apu->step(soundCpuCyclesActual, m_gameSpeed);
        }
        
        // Run PPU (graphics chip runs in parallel)
        // PPU typically runs at similar speed to main CPU
        u32 ppuCycles = cpuCycles;
        m_ppu->step(ppuCycles);

        // Check for IRQ timer
        u32 newCycles = m_cpu->getCycles();
        u32 targetIRQCycles = m_memory->getTargetIRQCycles();
        u16 irqControl = m_memory->getIRQControl();
        if (irqControl & 0x10) {
            s32 cyclesToIRQ = static_cast<s32>(targetIRQCycles) - static_cast<s32>(newCycles);
            if (cyclesToIRQ > 0 && cyclesToIRQ < static_cast<s32>(CPU_CYCLES_PER_STEP)) {
                nextCpuCycles = cyclesToIRQ;
            } else if (oldCycles < targetIRQCycles && newCycles >= targetIRQCycles) {
                m_memory->timerIRQ();
            }
        }
    }
    
    m_ppu->clearFrameComplete();

    // Update watchdog timer
    if (m_cartridge) {
        m_watchdogTimer += cyclesThisFrame;

        // Check if watchdog has expired (should trigger every ~8 frames)
        if (m_watchdogTimer > static_cast<s32>(WATCHDOG_TIMEOUT_CYCLES)) {
            log_error("Watchdog timer expired, resetting system...");
            reset();
            return;
        }
    }
}

bool Core::saveState(const fs::path& filename) {
    Buffer buf = {};
    
    // Save all component states
    m_cpu->saveState(&buf);
    m_soundCpu->saveState(&buf);
    m_apu->saveState(&buf);
    m_ppu->saveState(&buf);
    m_memory->saveState(&buf);
    m_cartridge->saveState(&buf);
    m_controller->saveState(&buf);
    m_upd4990a->saveState(&buf);
    
    return buffer_save_to_file(&buf, filename);
}

bool Core::loadState(const fs::path& filename) {
    Buffer buf = {};
    buffer_load_from_file(&buf, filename);
    
    // Load all component states
    m_cpu->loadState(&buf);
    m_soundCpu->loadState(&buf);
    m_apu->loadState(&buf);
    m_ppu->loadState(&buf);
    m_memory->loadState(&buf);
    m_cartridge->loadState(&buf);
    m_controller->loadState(&buf);
    m_upd4990a->loadState(&buf);
    
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
