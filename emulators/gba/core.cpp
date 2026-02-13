#include "core.h"
#include <SDL3/SDL.h>
#include "../components/buffer.h"
#include <cstdio>
#include <vector>

namespace gba {

Core::Core()
    : m_cyclesThisFrame(0) {
}

bool Core::initialize() {
    // Create core components
    m_cartridge = std::make_unique<Cartridge>();
    m_cpu = std::make_unique<CPU>();
    m_memory = std::make_unique<Memory>();
    m_ppu = std::make_unique<PPU>();
    m_joypad = std::make_unique<Joypad>();
    m_timer = std::make_unique<Timer>();
    m_dma = std::make_unique<DMA>();

    // Wire up components
    m_memory->setCartridge(m_cartridge.get());
    m_memory->setPPU(m_ppu.get());
    m_memory->setJoypad(m_joypad.get());
    m_memory->setTimer(m_timer.get());
    m_memory->setDMA(m_dma.get());
    
    m_cpu->setMemory(m_memory.get());
    m_ppu->setMemory(m_memory.get());
    m_ppu->setDMA(m_dma.get());
    m_joypad->setMemory(m_memory.get());
    m_timer->setMemory(m_memory.get());
    m_dma->setMemory(m_memory.get());

    return true;
}

void Core::setVideoDevice(VideoDevice* videoDevice) {
    if (m_ppu) {
        m_ppu->setVideoDevice(videoDevice);
    }
}

void Core::setAudioDevice(AudioDevice* audioDevice) {
    (void)audioDevice;
    // TODO: Implement audio
}

bool Core::loadBootrom(const fs::path& filename) {
    // GBA BIOS should be 16KB (0x4000 bytes)
    const u32 BIOS_SIZE = 0x4000;
    
    FILE* file = fopen(filename.string().c_str(), "rb");
    if (!file) {
        log_error("Failed to open BIOS file: %s", filename.string().c_str());
        return false;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (size != BIOS_SIZE) {
        log_error("Invalid BIOS size: %ld bytes. Expected %u bytes", size, BIOS_SIZE);
        fclose(file);
        return false;
    }
    
    // Read BIOS data
    std::vector<u8> biosData(BIOS_SIZE);
    size_t bytesRead = fread(biosData.data(), 1, BIOS_SIZE, file);
    fclose(file);
    
    if (bytesRead != BIOS_SIZE) {
        log_error("Failed to read BIOS data: read %zu of %u bytes", bytesRead, BIOS_SIZE);
        return false;
    }
    
    // Load BIOS into memory
    if (!m_memory->loadBIOS(biosData.data(), BIOS_SIZE)) {
        log_error("Failed to load BIOS into memory");
        return false;
    }
    
    return true;
}

bool Core::loadROM(const fs::path& filename) {
    if (!m_cartridge->load(filename)) {
        return false;
    }

    // Reset all components
    m_memory->reset();
    m_cpu->reset();
    m_ppu->reset();
    m_timer->reset();
    m_dma->reset();

    return true;
}

void Core::update() {
    // Run until we've executed enough cycles for one frame
    u32 targetCycles = static_cast<u32>(CYCLES_PER_FRAME / m_gameSpeed);
    
    while (m_cyclesThisFrame < targetCycles) {
        // Step CPU
        u32 cycles = m_cpu->step();
        
        // Step other components
        m_ppu->step(cycles);
        m_timer->step(cycles);
        
        // Run any triggered DMAs
        m_dma->runImmediate();
        
        m_cyclesThisFrame += cycles;
    }
    
    m_cyclesThisFrame -= targetCycles;
}

bool Core::saveState(const fs::path& filename) {
    Buffer buf = {};
    
    // Save all component states
    buffer_write(&buf, &m_cyclesThisFrame, sizeof(m_cyclesThisFrame));
    m_cpu->saveState(&buf);
    m_memory->saveState(&buf);
    m_ppu->saveState(&buf);
    m_timer->saveState(&buf);
    m_joypad->saveState(&buf);
    m_dma->saveState(&buf);
    m_cartridge->saveState(&buf);
    
    return buffer_save_to_file(&buf, filename);
}

bool Core::loadState(const fs::path& filename) {
    Buffer buf = {};
    buffer_load_from_file(&buf, filename);
    
    // Load all component states
    buffer_read(&buf, &m_cyclesThisFrame, sizeof(m_cyclesThisFrame));
    m_cpu->loadState(&buf);
    m_memory->loadState(&buf);
    m_ppu->loadState(&buf);
    m_timer->loadState(&buf);
    m_joypad->loadState(&buf);
    m_dma->loadState(&buf);
    m_cartridge->loadState(&buf);

    return true;
}

void Core::updateGameSpeed(double gameSpeed) {
    m_gameSpeed = gameSpeed;
}

} // namespace gba
