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
    m_audio = std::make_unique<Audio>();
    m_video = std::make_unique<Video>();
    m_memory = std::make_unique<Memory>();
    m_controller = std::make_unique<Controller>();
    m_upd4990a = std::make_unique<UPD4990A>();

    // Wire up components
    m_cpu->setMemory(m_memory.get());
    m_soundCpu->setMemory(m_memory.get());
    m_soundCpu->setAudio(m_audio.get());
    
    m_audio->setSoundCPU(m_soundCpu.get());
    m_audio->setCPU(m_cpu.get());
    m_audio->setMemory(m_memory.get());
    m_audio->setCartridge(m_cartridge.get());
    
    m_video->setCPU(m_cpu.get());
    m_video->setCartridge(m_cartridge.get());
    m_video->setMemory(m_memory.get());
    
    m_memory->setCPU(m_cpu.get());
    m_memory->setSoundCPU(m_soundCpu.get());
    m_memory->setVideo(m_video.get());
    m_memory->setCartridge(m_cartridge.get());
    m_memory->setController(m_controller.get());
    m_memory->setAudio(m_audio.get());
    m_memory->setUPD4990A(m_upd4990a.get());
    m_memory->setCore(this);
    
    m_cartridge->setCPU(m_cpu.get());
    m_cartridge->setVideo(m_video.get());

    m_controller->setCartridge(m_cartridge.get());

    return true;
}

void Core::setVideoDevice(::VideoDevice* videoDevice) {
    m_video->setVideoDevice(videoDevice);
}

void Core::setAudioDevice(::AudioDevice* audioDevice) {
    m_audio->setAudioDevice(audioDevice);
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
    m_audio->reset();
    m_video->reset();
    m_memory->reset();
    m_controller->reset();
    m_upd4990a->initialize(CPU_FREQUENCY, [this]() { return m_cpu->frameCycles(); });

    m_watchdogTimer = 0;
}

void Core::update() {
    // Run until we complete a frame
    m_video->newFrame();

    u32 nextCpuCycles = cyclesToNextEvent(m_cpu->frameCycles());

    while (m_cpu->frameCycles() < CPU_CYCLES_PER_FRAME) {
        // Execute CPU cycles
        u32 oldCycles = m_cpu->frameCycles();
        u32 cpuCycles = m_cpu->step(nextCpuCycles);
        u32 newCycles = m_cpu->frameCycles();

        // Run Video
        m_video->step(cpuCycles);

        // Check for IRQ timer
        u32 targetIRQCycles = m_memory->getTargetIRQCycles();
        u16 irqControl = m_memory->getIRQControl();
        if ((irqControl & 0x10) &&
            // Skip timer IRQ for zedblade
            m_cartridge->getGameId() != 0x76) {
            if (oldCycles < targetIRQCycles && newCycles >= targetIRQCycles) {
                m_memory->timerIRQ();
                m_video->renderSprites();
            }
        }

        // Compute step size to land precisely on the next event
        nextCpuCycles = cyclesToNextEvent(newCycles);
    }

    // Update watchdog timer
    m_watchdogTimer += m_cpu->frameCycles();

    // Check if watchdog has expired (should trigger every ~8 frames)
    if (m_watchdogTimer > static_cast<s32>(WATCHDOG_TIMEOUT_CYCLES)) {
        log_info("Watchdog timer expired, resetting system...");
        reset();
        return;
    }

    m_upd4990a->update();
    m_audio->endFrame(m_gameSpeed);
    m_cpu->endFrame();
    m_soundCpu->endFrame();
    m_memory->endFrame();
    m_upd4990a->endFrame(m_cpu->frameCycles());
}

bool Core::saveState(const fs::path& filename) {
    Buffer buf = {};
    
    // Save all component states
    buffer_write(&buf, &m_watchdogTimer, sizeof(m_watchdogTimer));
    m_cpu->saveState(&buf);
    m_soundCpu->saveState(&buf);
    m_audio->saveState(&buf);
    m_video->saveState(&buf);
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
    buffer_read(&buf, &m_watchdogTimer, sizeof(m_watchdogTimer));
    m_cpu->loadState(&buf);
    m_soundCpu->loadState(&buf);
    m_audio->loadState(&buf);
    m_video->loadState(&buf);
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
    m_audio->setSampleRate(sampleRate);
}

void Core::setAudioVolume(float volume) {
    m_audio->setVolume(volume);
}

u32 Core::cyclesToNextEvent(u32 currentCycles) const {
    // Start with cycles to end of frame
    u32 next = CPU_CYCLES_PER_FRAME - currentCycles;

    // Cycles to next scanline boundary
    u32 toScanline = m_video->cyclesToNextScanline();
    if (toScanline < next) {
        next = toScanline;
    }

    // Cycles to timer IRQ
    u16 irqControl = m_memory->getIRQControl();
    if ((irqControl & 0x10) && m_cartridge->getGameId() != 0x76) {
        u32 targetIRQCycles = m_memory->getTargetIRQCycles();
        if (targetIRQCycles > currentCycles) {
            u32 toIRQ = targetIRQCycles - currentCycles;
            if (toIRQ < next) {
                next = toIRQ;
            }
        }
    }

    // Ensure we always step at least 1 cycle
    return next > 0 ? next : 1;
}

} // namespace neogeo
