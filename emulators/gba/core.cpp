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
    m_apu = std::make_unique<APU>();
    m_gpio = std::make_unique<GPIO>();

    // Wire up components
    m_memory->setCartridge(m_cartridge.get());
    m_memory->setPPU(m_ppu.get());
    m_memory->setJoypad(m_joypad.get());
    m_memory->setTimer(m_timer.get());
    m_memory->setDMA(m_dma.get());
    m_memory->setAPU(m_apu.get());
    
    m_cpu->setMemory(m_memory.get());
    m_ppu->setMemory(m_memory.get());
    m_ppu->setDMA(m_dma.get());
    m_joypad->setMemory(m_memory.get());
    m_timer->setMemory(m_memory.get());
    m_timer->setAPU(m_apu.get());
    m_dma->setMemory(m_memory.get());
    m_apu->setMemory(m_memory.get());
    m_apu->setTimer(m_timer.get());
    m_apu->setDMA(m_dma.get());
    m_memory->setGPIO(m_gpio.get());

    return true;
}

void Core::setVideoDevice(VideoDevice* videoDevice) {
    m_ppu->setVideoDevice(videoDevice);
}

void Core::setAudioDevice(AudioDevice* audioDevice) {
    m_apu->setAudioDevice(audioDevice);
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

    if (!m_memory->hasBIOS()) {
        fs::path biosPath = filename.parent_path() / "gba_bios.bin";
        if (fs::exists(biosPath)) {
            loadBootrom(biosPath);
        }
    }

    if (!m_memory->hasBIOS()) {
        #ifdef __EMSCRIPTEN__
        log_error("Failed to load BIOS. Upload gba_bios.bin and try again.");
        #else
        log_error("Failed to load BIOS. Please specify --bios and try again.");
        #endif
        return false;
    }

    // Reset all components
    m_memory->reset();
    m_cpu->reset();
    m_ppu->reset();
    m_timer->reset();
    m_dma->reset();
    m_apu->reset();

    // Set up GPIO after cartridge is loaded
    m_gpio->setROM(m_cartridge->getROM(), m_cartridge->getROMSize());
    m_gpio->reset();

    return true;
}

void Core::update() {
    // Run until we've executed enough cycles for one frame
    while (m_cyclesThisFrame < CYCLES_PER_FRAME) {
        // Step CPU
        u32 cycles = m_cpu->step(1);
        
        // Step other components
        m_ppu->step(cycles);
        m_timer->step(cycles);
        m_apu->step(cycles, m_gameSpeed);
        
        m_cyclesThisFrame += cycles;
    }
    
    m_cyclesThisFrame -= CYCLES_PER_FRAME;
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
    m_apu->saveState(&buf);
    m_cartridge->saveState(&buf);
    m_gpio->saveState(&buf);
    
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
    m_apu->loadState(&buf);
    m_cartridge->loadState(&buf);
    m_gpio->loadState(&buf);

    return true;
}

void Core::updateGameSpeed(double gameSpeed) {
    m_gameSpeed = gameSpeed;
}

} // namespace gba
