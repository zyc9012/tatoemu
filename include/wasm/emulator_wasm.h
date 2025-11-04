#pragma once

#include "types.h"
#include "cpu.h"
#include "mmu.h"
#include "ppu.h"
#include "joypad.h"
#include "timer.h"
#include "cartridge.h"
#include "apu.h"
#include "bootrom.h"
#include <memory>
#include <string>
#include <vector>

// WASM Video Device - renders to a buffer that JavaScript can read
class WasmVideoDevice : public VideoDevice {
public:
    WasmVideoDevice();
    ~WasmVideoDevice();

    void render(u32* buffer) override;
    u32* getFrameBuffer() { return m_frameBuffer; }

private:
    u32 m_frameBuffer[SCREEN_WIDTH * SCREEN_HEIGHT];
};

// WASM Audio Device - queues audio samples that JavaScript can read
class WasmAudioDevice : public AudioDevice {
public:
    WasmAudioDevice();
    ~WasmAudioDevice();

    bool initialize();
    void shutdown();
    void clearBuffer();
    int getQueuedSize() const;
    
    void writeSamples(void* stream, u32 length) override;
    
    // Get audio samples for JavaScript
    int getSamples(float* buffer, int maxSamples);

private:
    std::vector<float> m_audioBuffer;
    const size_t m_maxBufferSize = 48000 * 2; // 1 second of stereo audio at 48kHz
};

class EmulatorWasm {
public:
    EmulatorWasm();
    ~EmulatorWasm();

    bool initialize();
    bool loadBootromFromMemory(const u8* data, size_t size);
    bool loadROMFromMemory(const u8* data, size_t size);
    void shutdown();
    
    // Main loop - call this once per frame from JavaScript
    void runFrame();
    
    // Input handling
    void pressButton(u8 button);
    void releaseButton(u8 button);
    
    // Access to frame buffer for rendering
    u32* getFrameBuffer();
    
    // Audio
    int getAudioSamples(float* buffer, int maxSamples);
    void clearAudioBuffer();
    int getQueuedAudioSize() const;
    
    // Save/Load state
    bool saveState(std::vector<u8>& data);
    bool loadState(const std::vector<u8>& data);
    
    // Info
    std::string getCartridgeTitle() const;
    bool isRunning() const { return m_running; }
    
private:
    void update();

    // Emulator components
    std::unique_ptr<CPU> m_cpu;
    std::unique_ptr<MMU> m_mmu;
    std::unique_ptr<PPU> m_ppu;
    std::unique_ptr<Joypad> m_joypad;
    std::unique_ptr<Timer> m_timer;
    std::unique_ptr<Cartridge> m_cartridge;
    std::unique_ptr<APU> m_apu;
    std::unique_ptr<Bootrom> m_bootrom;
    std::unique_ptr<WasmVideoDevice> m_videoDevice;
    std::unique_ptr<WasmAudioDevice> m_audioDevice;
    
    bool m_running;
    u32 m_cyclesThisFrame;
};

