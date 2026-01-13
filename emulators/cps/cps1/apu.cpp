#include "apu.h"
#include "../sound_cpu.h"
#include "memory.h"
#include "cartridge.h"
#include "../../types.h"
#include "../../components/sound/state.h"
#include <cstring>
#include <cmath>
#include <algorithm>
#include "consts.h"

namespace cps1 {

// CPS1 Z80 I/O Port Map:
// 0x00-0x01: YM2151 (address/data)
// 0x02-0x03: MSM6295 (command/status)

// Static pointer to SoundCPU for interrupt handler callback
// This is set when setSoundCPU() is called
static cps::SoundCPU* s_ym2151SoundCpu = nullptr;

// YM2151 interrupt handler - sets/clears Z80 INT line based on YM2151 interrupt status
static void ym2151IrqHandler(INT32 nStatus) {
    if (s_ym2151SoundCpu) {
        s_ym2151SoundCpu->irq(nStatus != 0);
    }
}

APU::APU()
    : m_soundCpu(nullptr)
    , m_memory(nullptr)
    , m_cartridge(nullptr)
    , m_audioDevice(nullptr)
    , m_cycleAccumulator(0)
    , m_cyclesPerSample(0)
    , m_ym2151RegSelect(0) {

    m_ym2151LeftBuffer.resize(0x100);
    m_ym2151RightBuffer.resize(0x100);
    m_msm6295Buffer.resize(0x100 * 2);

    // Initialize YM2151
    YM2151Init(1, 0, 3579540, 44100, nullptr);
    
    // Set YM2151 interrupt handler (will be connected to Z80 when setSoundCPU is called)
    YM2151SetIrqHandler(0, ym2151IrqHandler);
    
    // Initialize MSM6295
    MSM6295Init(0, 7576, false);
    MSM6295SetRoute(0, 1, BURN_SND_ROUTE_BOTH);
}

APU::~APU() {
    YM2151Shutdown();
    MSM6295Exit(0);
}

void APU::setSoundCPU(cps::SoundCPU* soundCpu) {
    m_soundCpu = soundCpu;
    // Update the static pointer in the IRQ handler so it can trigger Z80 interrupts
    s_ym2151SoundCpu = soundCpu;
}

void APU::setMemory(cps::MemoryBase* memory) {
    m_memory = static_cast<Memory*>(memory);
}

void APU::setCartridge(Cartridge* cartridge) {
    m_cartridge = cartridge;
    setROMData();
}

void APU::reset() {
    // Reset sound chips
    YM2151ResetChip(0);
    MSM6295Reset(0);
    m_ym2151RegSelect = 0;
    
    // Reset sample generation
    m_cycleAccumulator = 0;
    
    // Calculate cycles per sample (Z80 runs at 4 MHz)
    m_cyclesPerSample = SOUND_CPU_FREQUENCY / m_sampleRate;
    
    // Set ROM data for MSM6295
    setROMData();
}

void APU::setROMData() {
    if (!m_cartridge) {
        return;
    }
    
    // Get sound sample data from cartridge (MSM6295 only needs samples, not Z80 program code)
    const u8* sampleData = m_cartridge->getSoundSample();
    u32 sampleSize = m_cartridge->getSoundSampleSize();
    
    if (sampleData && sampleSize > 0) {
        // Set bank
        INT32 endAddr = static_cast<INT32>(sampleSize - 1);
        MSM6295SetBank(0, const_cast<UINT8*>(sampleData), 0, endAddr);
    }
}

void APU::step(u32 cycles, double gameSpeed) {
    (void)gameSpeed;
    generateSamples(cycles);
}

void APU::setSampleRate(u32 sampleRate) {
    m_sampleRate = sampleRate;
    nBurnSoundRate = sampleRate;
    
    YM2151SetSampleRate(0, sampleRate);
    MSM6295SetSamplerate(0, 7576);
    
    // Recalculate cycles per sample
    m_cyclesPerSample = SOUND_CPU_FREQUENCY / m_sampleRate;
}

void APU::setVolume(float volume) {
    m_volume = volume;
}

u8 APU::readPort(u16 port) {
    // YM2151 status register (port 0x01)
    if (port == 0x01) {
        return static_cast<u8>(YM2151ReadStatus(0));
    }
    
    // MSM6295 status (port 0x02)
    if (port == 0x02) {
        return static_cast<u8>(MSM6295Read(0));
    }
    
    return 0xFF;
}

void APU::writePort(u16 port, u8 value) {
    // YM2151 register select (port 0x00)
    if (port == 0x00) {
        m_ym2151RegSelect = value;
        return;
    }
    
    // YM2151 data write (port 0x01)
    if (port == 0x01) {
        YM2151WriteReg(0, m_ym2151RegSelect, value);
        return;
    }
    
    // MSM6295 command (port 0x02)
    if (port == 0x02) {
        MSM6295Write(0, value);
        return;
    }
}

void APU::generateSamples(u32 cycles) {
    if (!m_audioDevice) {
        return;
    }
    
    // Accumulate cycles and calculate how many samples to generate
    m_cycleAccumulator += cycles;
    u32 numSamples = static_cast<u32>(m_cycleAccumulator / m_cyclesPerSample);
    m_cycleAccumulator %= m_cyclesPerSample;
    
    if (numSamples == 0) {
        return;
    }

    if (numSamples > m_ym2151LeftBuffer.size()) {
        m_ym2151LeftBuffer.resize(numSamples);
        m_ym2151RightBuffer.resize(numSamples);
        m_msm6295Buffer.resize(numSamples * 2);
    }
    
    INT16* ym2151Buffers[2] = { m_ym2151LeftBuffer.data(), m_ym2151RightBuffer.data() };
    
    // Generate YM2151 samples
    YM2151UpdateOne(0, ym2151Buffers, static_cast<int>(numSamples));

    // Generate MSM6295 samples
    MSM6295Render(0, m_msm6295Buffer.data(), static_cast<INT32>(numSamples));

    // Mix and convert to float
    for (u32 i = 0; i < numSamples; i++) {
        // Convert YM2151 samples to float
        float ymLeft = static_cast<float>(m_ym2151LeftBuffer[i]) / 32768.0f;
        float ymRight = static_cast<float>(m_ym2151RightBuffer[i]) / 32768.0f;
        
        // Convert MSM6295 samples to float
        float msmLeft = static_cast<float>(m_msm6295Buffer[i]) / 32768.0f;
        float msmRight = static_cast<float>(m_msm6295Buffer[i + 1]) / 32768.0f;
        
        // Mix channels
        float left = ymLeft * 0.35 + msmLeft * 0.30;
        float right = ymRight * 0.35 + msmRight * 0.30;
        
        // Apply volume
        left *= m_volume;
        right *= m_volume;
        
        // Clamp to [-1.0, 1.0]
        left = std::clamp(left, -1.0f, 1.0f);
        right = std::clamp(right, -1.0f, 1.0f);
        
        float samples[2] = {left, right};
        m_audioDevice->writeSamples(samples, sizeof(samples));
    }
}

void APU::saveState(std::ofstream& file) {
    file.write(reinterpret_cast<const char*>(&m_sampleRate), sizeof(m_sampleRate));
    file.write(reinterpret_cast<const char*>(&m_volume), sizeof(m_volume));
    file.write(reinterpret_cast<const char*>(&m_cycleAccumulator), sizeof(m_cycleAccumulator));
    file.write(reinterpret_cast<const char*>(&m_cyclesPerSample), sizeof(m_cyclesPerSample));
    file.write(reinterpret_cast<const char*>(&m_ym2151RegSelect), sizeof(m_ym2151RegSelect));
    INT32 nAction = ACB_READ;
    BurnYM2151Scan_int(nAction);
    INT32 pnMin = 0;
    MSM6295Scan(nAction, &pnMin);
}

void APU::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(&m_sampleRate), sizeof(m_sampleRate));
    file.read(reinterpret_cast<char*>(&m_volume), sizeof(m_volume));
    file.read(reinterpret_cast<char*>(&m_cycleAccumulator), sizeof(m_cycleAccumulator));
    file.read(reinterpret_cast<char*>(&m_cyclesPerSample), sizeof(m_cyclesPerSample));
    file.read(reinterpret_cast<char*>(&m_ym2151RegSelect), sizeof(m_ym2151RegSelect));
    INT32 nAction = ACB_WRITE;
    BurnYM2151Scan_int(nAction);
    INT32 pnMin = 0;
    MSM6295Scan(nAction, &pnMin);
}

} // namespace cps1
