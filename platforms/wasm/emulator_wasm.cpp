#include <iostream>
#include <cstring>
#include <sstream>
#include "emulator_wasm.h"

WasmVideoDevice::WasmVideoDevice() {
    // Initialize frame buffer to white
    std::memset(m_frameBuffer, 0xFF, sizeof(m_frameBuffer));
}

WasmVideoDevice::~WasmVideoDevice() {
}

void WasmVideoDevice::render(u32* buffer) {
    // Copy the buffer to our internal frame buffer
    std::memcpy(m_frameBuffer, buffer, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(u32));
}

WasmAudioDevice::WasmAudioDevice() {
    m_audioBuffer.reserve(m_maxBufferSize);
}

WasmAudioDevice::~WasmAudioDevice() {
    shutdown();
}

bool WasmAudioDevice::initialize() {
    m_audioBuffer.clear();
    return true;
}

void WasmAudioDevice::shutdown() {
    m_audioBuffer.clear();
}

void WasmAudioDevice::clearBuffer() {
    m_audioBuffer.clear();
}

int WasmAudioDevice::getQueuedSize() const {
    return static_cast<int>(m_audioBuffer.size() * sizeof(float));
}

void WasmAudioDevice::writeSamples(void* stream, u32 length) {
    float* samples = static_cast<float*>(stream);
    u32 numSamples = length / sizeof(float);
    
    // Add samples to buffer, but don't exceed max size
    for (u32 i = 0; i < numSamples && m_audioBuffer.size() < m_maxBufferSize; i++) {
        m_audioBuffer.push_back(samples[i]);
    }
}

int WasmAudioDevice::getSamples(float* buffer, int maxSamples) {
    int samplesToReturn = std::min(maxSamples, static_cast<int>(m_audioBuffer.size()));
    
    if (samplesToReturn > 0) {
        std::memcpy(buffer, m_audioBuffer.data(), samplesToReturn * sizeof(float));
        m_audioBuffer.erase(m_audioBuffer.begin(), m_audioBuffer.begin() + samplesToReturn);
    }
    
    return samplesToReturn;
}

EmulatorWasm::EmulatorWasm()
    : m_running(false)
    , m_cyclesThisFrame(0) {
}

EmulatorWasm::~EmulatorWasm() {
    shutdown();
}

bool EmulatorWasm::initialize() {
    // Create emulator components
    m_cartridge = std::make_unique<Cartridge>();
    m_cpu = std::make_unique<CPU>();
    m_mmu = std::make_unique<MMU>();
    m_ppu = std::make_unique<PPU>();
    m_joypad = std::make_unique<Joypad>();
    m_timer = std::make_unique<Timer>();
    m_apu = std::make_unique<APU>();
    m_bootrom = std::make_unique<Bootrom>();
    m_videoDevice = std::make_unique<WasmVideoDevice>();
    m_audioDevice = std::make_unique<WasmAudioDevice>();

    // Wire up components
    m_mmu->setCartridge(m_cartridge.get());
    m_mmu->setPPU(m_ppu.get());
    m_mmu->setJoypad(m_joypad.get());
    m_mmu->setTimer(m_timer.get());
    m_mmu->setAPU(m_apu.get());
    m_mmu->setBootrom(m_bootrom.get());
    
    m_cpu->setMMU(m_mmu.get());
    m_ppu->setCPU(m_cpu.get());
    m_ppu->setMMU(m_mmu.get());
    m_ppu->setVideoDevice(m_videoDevice.get());
    m_joypad->setCPU(m_cpu.get());
    m_timer->setCPU(m_cpu.get());
    m_timer->setMMU(m_mmu.get());
    m_apu->setCPU(m_cpu.get());
    m_apu->setMMU(m_mmu.get());
    m_apu->setAudioDevice(m_audioDevice.get());

    // Initialize audio
    if (!m_audioDevice->initialize()) {
        std::cerr << "Warning: Failed to initialize audio" << std::endl;
        // Continue anyway - emulator can run without audio
    }

    std::cout << "WASM Emulator initialized successfully" << std::endl;
    return true;
}

bool EmulatorWasm::loadBootromFromMemory(const u8* data, size_t size) {
    if (!m_bootrom->loadFromMemory(data, size)) {
        std::cerr << "Failed to load bootrom, continuing without it" << std::endl;
        return false;
    }
    return true;
}

bool EmulatorWasm::loadROMFromMemory(const u8* data, size_t size) {
    if (!m_cartridge->loadFromMemory(data, size)) {
        return false;
    }

    // Enable GBC mode if cartridge supports it
    bool isGBC = m_cartridge->isGBC();
    m_cpu->setGBCMode(isGBC);
    m_ppu->setGBCMode(isGBC);
    m_mmu->setGBCMode(isGBC);

    // Reset bootrom to enabled state if loaded
    if (m_bootrom->isLoaded()) {
        m_bootrom->reset();
    }

    // Reset CPU after loading ROM
    // If bootrom is loaded, start from 0x0000; otherwise skip to 0x0100
    bool useBootrom = m_bootrom->isLoaded();
    m_cpu->reset(useBootrom);
    m_ppu->reset(useBootrom);
    m_timer->reset();
    m_apu->reset();

    m_running = true;
    m_cyclesThisFrame = 0;

    return true;
}

void EmulatorWasm::runFrame() {
    if (!m_running) {
        return;
    }

    update();
}

void EmulatorWasm::update() {
    // In double speed mode, CPU runs at 2x speed, so we need 2x cycles per frame
    // to maintain the same real-time frame rate
    u32 targetCycles = m_mmu->isDoubleSpeed() ? (CYCLES_PER_FRAME * 2) : CYCLES_PER_FRAME;
    
    // Run until we've executed enough cycles for one frame
    while (m_cyclesThisFrame < targetCycles) {
        u32 cycles = m_cpu->step();
        
        m_ppu->step(cycles);
        m_timer->step(cycles);
        m_apu->step(cycles);
        
        // Add DMA cycles if any DMA occurred
        u32 dmaCycles = m_ppu->getDMACycles();
        if (dmaCycles > 0) {
            m_ppu->clearDMACycles();
            cycles += dmaCycles;
        }
        
        m_cyclesThisFrame += cycles;
    }
    
    m_cyclesThisFrame -= targetCycles;
}

void EmulatorWasm::shutdown() {
    // Close audio first to prevent issues
    if (m_audioDevice) {
        m_audioDevice->shutdown();
    }
    
    m_running = false;
}

void EmulatorWasm::pressButton(u8 button) {
    if (m_joypad) {
        m_joypad->pressButton(static_cast<JoypadButton>(button));
    }
}

void EmulatorWasm::releaseButton(u8 button) {
    if (m_joypad) {
        m_joypad->releaseButton(static_cast<JoypadButton>(button));
    }
}

u32* EmulatorWasm::getFrameBuffer() {
    return m_videoDevice->getFrameBuffer();
}

int EmulatorWasm::getAudioSamples(float* buffer, int maxSamples) {
    return m_audioDevice->getSamples(buffer, maxSamples);
}

void EmulatorWasm::clearAudioBuffer() {
    m_audioDevice->clearBuffer();
}

int EmulatorWasm::getQueuedAudioSize() const {
    return m_audioDevice->getQueuedSize();
}

bool EmulatorWasm::saveState(std::vector<u8>& data) {
    // TODO: Implement save state for WASM
    // Would need to refactor saveState methods to accept std::ostream instead of std::ofstream
    // For now, return false to indicate not implemented
    (void)data; // Suppress unused parameter warning
    return false;
}

bool EmulatorWasm::loadState(const std::vector<u8>& data) {
    // TODO: Implement load state for WASM
    // Would need to refactor loadState methods to accept std::istream instead of std::ifstream
    // For now, return false to indicate not implemented
    (void)data; // Suppress unused parameter warning
    return false;
}

std::string EmulatorWasm::getCartridgeTitle() const {
    if (m_cartridge) {
        return m_cartridge->getTitle();
    }
    return "";
}

