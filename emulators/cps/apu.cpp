#include "apu.h"
#include "sound_cpu.h"
#include "memory.h"
#include "cartridge.h"
#include "../types.h"
#include "../components/compact.h"
#include <cstring>
#include <cmath>
#include <algorithm>
#include "consts.h"

namespace cps {

// Static pointer to SoundCPU for interrupt handler callback
// This is set when setSoundCPU() is called
static SoundCPU* s_ym2151SoundCpu = nullptr;

// YM2151 interrupt handler - sets/clears Z80 INT line based on YM2151 interrupt status
static void ym2151IrqHandler(s32 nStatus) {
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
}

APU::~APU() {
    if (m_cartridge->getCPSVersion() == 1) {
        YM2151Shutdown();
        MSM6295Exit(0);
    } else {
        QscExit();
    }
}

void APU::setSoundCPU(SoundCPU* soundCpu) {
    m_soundCpu = soundCpu;
    // Update the static pointer in the IRQ handler so it can trigger Z80 interrupts
    s_ym2151SoundCpu = soundCpu;
}

void APU::setMemory(Memory* memory) {
    m_memory = memory;
}

void APU::setCartridge(Cartridge* cartridge) {
    m_cartridge = cartridge;
}

void APU::reset() {
    // Reset sound chips
    if (m_cartridge->getCPSVersion() == 1) {
         // Initialize YM2151
        YM2151Init(1, 0, 3579540, 44100, nullptr);
        
        // Set YM2151 interrupt handler (will be connected to Z80 when setSoundCPU is called)
        YM2151SetIrqHandler(0, ym2151IrqHandler);
        
        // Initialize MSM6295
        MSM6295Init(0, 7576, false, 44100, 0);
        MSM6295SetRoute(0, 1, BURN_SND_ROUTE_BOTH);

        YM2151ResetChip(0);
        MSM6295Reset(0);

        m_ym2151RegSelect = 0;

        m_cyclesPerSample = cps1::SOUND_CPU_FREQUENCY / m_sampleRate;
    } else {
        // Initialize QSound
        QscInit(44100);
        QscSetRoute(BURN_SND_QSND_OUTPUT_1, 1.0, BURN_SND_ROUTE_LEFT);
        QscSetRoute(BURN_SND_QSND_OUTPUT_2, 1.0, BURN_SND_ROUTE_RIGHT);
        QscReset();

        m_cyclesPerSample = cps2::SOUND_CPU_FREQUENCY / m_sampleRate;
    }

    // Reset sample generation
    m_cycleAccumulator = 0;

    // Set ROM data
    setROMData();
}

void APU::setROMData() {
    if (!m_cartridge) {
        return;
    }

    // Get sound sample data from cartridge
    const u8* sampleData = m_cartridge->getSoundSample();
    u32 sampleSize = m_cartridge->getSoundSampleSize();

    if (sampleData && sampleSize > 0) {
        if (m_cartridge->getCPSVersion() == 1) {
            // CPS1: MSM6295 samples
            s32 endAddr = static_cast<s32>(sampleSize - 1);
            MSM6295SetBank(0, const_cast<u8*>(sampleData), 0, endAddr);
    } else {
        // CPS2: QSound samples
        QscSetSampleROM(const_cast<u8*>(sampleData), sampleSize);
    }
    }
}

void APU::step(u32 cycles, double gameSpeed) {
    (void)gameSpeed;
    generateSamples(cycles, gameSpeed);
}

void APU::setSampleRate(u32 sampleRate) {
    m_sampleRate = sampleRate;

    if (m_cartridge->getCPSVersion() == 1) {
        // CPS1: YM2151 and MSM6295
        YM2151SetSampleRate(0, sampleRate);
        MSM6295SetSamplerate(0, 7576, static_cast<s32>(sampleRate));
        m_cyclesPerSample = cps1::SOUND_CPU_FREQUENCY / m_sampleRate;
    } else {
        // CPS2: QSound - sample rate is fixed internally, but we can update our timing
        m_cyclesPerSample = cps2::SOUND_CPU_FREQUENCY / m_sampleRate;
    }
}

void APU::setVolume(float volume) {
    m_volume = volume;
}

u8 APU::readPort(u16 port) {
    if (m_cartridge->getCPSVersion() == 1) {
        // CPS1: YM2151 and MSM6295
        // YM2151 status register (port 0x01)
        if (port == 0x01) {
            return static_cast<u8>(YM2151ReadStatus(0));
        }
        // MSM6295 status register (port 0x02)
        if (port == 0x02) {
            return static_cast<u8>(MSM6295Read(0));
        }
    }

    return 0xFF;
}

u8 APU::readQSound() {
    return QscRead();
}

void APU::writePort(u16 port, u8 value) {
    if (m_cartridge->getCPSVersion() == 1) {
        // CPS1: YM2151 and MSM6295
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
        // MSM6295 write (port 0x02)
        if (port == 0x02) {
            MSM6295Write(0, value);
            return;
        }
    }
}

void APU::writeQSound(u16 port, u16 value) {
    QscWrite(port, value);
}

void APU::generateSamples(u32 cycles, double gameSpeed) {
    if (!m_audioDevice) {
        return;
    }

    // Accumulate cycles and calculate how many samples to generate
    m_cycleAccumulator += cycles;
    if (m_cycleAccumulator < m_cyclesPerSample * gameSpeed) {
        return;
    }
    m_cycleAccumulator -= m_cyclesPerSample * gameSpeed;

    float left = 0.0f, right = 0.0f;

    if (m_cartridge->getCPSVersion() == 1) {
        // CPS1: YM2151 + MSM6295
        s16* ym2151Buffers[2] = { &m_ym2151LeftSample, &m_ym2151RightSample };

        // Generate YM2151 samples
        YM2151UpdateOne(0, ym2151Buffers, 1);

        // Generate MSM6295 samples
        MSM6295Render(0, m_msm6295Samples, 1);

        // Convert and mix samples
        float ymLeft = static_cast<float>(m_ym2151LeftSample) / 32768.0f;
        float ymRight = static_cast<float>(m_ym2151RightSample) / 32768.0f;

        float msmLeft = static_cast<float>(m_msm6295Samples[0]) / 32768.0f;
        float msmRight = static_cast<float>(m_msm6295Samples[1]) / 32768.0f;

        // Mix channels
        left = ymLeft * 0.35f + msmLeft * 0.30f;
        right = ymRight * 0.35f + msmRight * 0.30f;
    } else {
        // CPS2: QSound
        // Generate QSound samples
        QscUpdate(1); // Generate one sample
        m_qsoundSamples[0] = QscGetLeftSample();
        m_qsoundSamples[1] = QscGetRightSample();

        // Convert QSound samples to float
        left = static_cast<float>(m_qsoundSamples[0]) / 32768.0f;
        right = static_cast<float>(m_qsoundSamples[1]) / 32768.0f;
    }

    // Apply volume
    left *= m_volume;
    right *= m_volume;

    // Clamp to [-1.0, 1.0]
    left = std::clamp(left, -1.0f, 1.0f);
    right = std::clamp(right, -1.0f, 1.0f);

    float samples[2] = {left, right};
    m_audioDevice->writeSamples(samples, sizeof(samples));
}

void APU::saveState(std::ofstream& file) {
    file.write(reinterpret_cast<const char*>(&m_sampleRate), sizeof(m_sampleRate));
    file.write(reinterpret_cast<const char*>(&m_volume), sizeof(m_volume));
    file.write(reinterpret_cast<const char*>(&m_cycleAccumulator), sizeof(m_cycleAccumulator));
    file.write(reinterpret_cast<const char*>(&m_cyclesPerSample), sizeof(m_cyclesPerSample));
    file.write(reinterpret_cast<const char*>(&m_ym2151RegSelect), sizeof(m_ym2151RegSelect));

    if (m_cartridge->getCPSVersion() == 1) {
        // Save YM2151 state
        YM2151SaveContext(file);
        // Save MSM6295 state
        MSM6295SaveContext(file);
    } else {
        // Save QSound state
        QscSaveContext(file);
    }
}

void APU::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(&m_sampleRate), sizeof(m_sampleRate));
    file.read(reinterpret_cast<char*>(&m_volume), sizeof(m_volume));
    file.read(reinterpret_cast<char*>(&m_cycleAccumulator), sizeof(m_cycleAccumulator));
    file.read(reinterpret_cast<char*>(&m_cyclesPerSample), sizeof(m_cyclesPerSample));
    file.read(reinterpret_cast<char*>(&m_ym2151RegSelect), sizeof(m_ym2151RegSelect));

    if (m_cartridge->getCPSVersion() == 1) {
        // Load YM2151 state
        YM2151LoadContext(file);
        // Load MSM6295 state
        MSM6295LoadContext(file);
    } else {
        // Load QSound state
        QscLoadContext(file);
    }
}

} // namespace cps
