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
    m_video = std::make_unique<Video>();
    m_audio = std::make_unique<Audio>();
    m_memory = std::make_unique<Memory>();
    m_controller = std::make_unique<Controller>();

    // Wire up components
    m_cpu->setMemory(m_memory.get());
    m_cpu->setCartridge(m_cartridge.get());
    m_soundCpu->setMemory(m_memory.get());
    m_soundCpu->setAudio(m_audio.get());
    m_soundCpu->setCartridge(m_cartridge.get());

    m_video->setCPU(m_cpu.get());
    m_video->setCartridge(m_cartridge.get());
    m_video->setMemory(m_memory.get());
    
    m_audio->setSoundCPU(m_soundCpu.get());
    m_audio->setCPU(m_cpu.get());
    m_audio->setMemory(m_memory.get());
    m_audio->setCartridge(m_cartridge.get());
    
    m_memory->setCPU(m_cpu.get());
    m_memory->setSoundCPU(m_soundCpu.get());
    m_memory->setVideo(m_video.get());
    m_memory->setAudio(m_audio.get());
    m_memory->setCartridge(m_cartridge.get());
    m_memory->setController(m_controller.get());
    
    m_cartridge->setCPU(m_cpu.get());
    m_cartridge->setVideo(m_video.get());

    return true;
}

void Core::setVideoDevice(::VideoDevice* videoDevice) {
    m_video->setVideoDevice(videoDevice);
}

void Core::setAudioDevice(::AudioDevice* audioDevice) {
    m_audio->setAudioDevice(audioDevice);
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
    m_video->reset();
    m_audio->reset();
    m_memory->reset();
    m_controller->reset();

    return true;
}

void Core::update() {
    // Run until we complete a frame

    u32 nextCpuCycles = cyclesToNextEvent(m_cpu->frameCycles());
    
    while (m_cpu->frameCycles() < m_cpu->cyclesPerFrame()) {
        // Execute CPU cycles
        u32 cpuCycles = m_cpu->step(nextCpuCycles);
        
        // Run Video
        m_video->step(cpuCycles);

        // Compute step size to land precisely on the next event
        nextCpuCycles = cyclesToNextEvent(m_cpu->frameCycles());
    }

    m_audio->endFrame(m_gameSpeed);
    m_cpu->endFrame();
    m_soundCpu->endFrame();
}

bool Core::saveState(const fs::path& filename) {
    Buffer buf = {};
    
    // Save all component states
    m_cpu->saveState(&buf);
    m_soundCpu->saveState(&buf);
    m_video->saveState(&buf);
    m_audio->saveState(&buf);
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
    m_video->loadState(&buf);
    m_audio->loadState(&buf);
    m_memory->loadState(&buf);
    m_cartridge->loadState(&buf);
    m_controller->loadState(&buf);
    
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
    // Cycles to end of frame
    u32 next = m_cpu->cyclesPerFrame() - currentCycles;

    // Cycles to next scanline boundary
    u32 toScanline = m_video->cyclesToNextScanline();
    if (toScanline < next) {
        next = toScanline;
    }

    return next > 0 ? next : 1;
}

} // namespace cps
