#include "audio.h"
#include "cpu.h"
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
static SoundCPU* s_ym2151SoundCpu = nullptr;

// YM2151 interrupt handler - sets/clears Z80 INT line based on YM2151 interrupt status
static void ym2151IrqHandler(s32 nStatus) {
    s_ym2151SoundCpu->irq(nStatus != 0);
}

Audio::Audio()
    : m_soundCpu(nullptr)
    , m_cpu(nullptr)
    , m_memory(nullptr)
    , m_cartridge(nullptr)
    , m_audioDevice(nullptr)
    , m_sampleRate(44100)
    , m_volume(1.0f)
    , m_ym2151RegSelect(0)
    , m_soundCPUCyclesPerFrame(0)
    , m_soundCPUFrequency(0)
    , m_soundCyclesPerSample(0) {
}

Audio::~Audio() {
    if (m_cartridge->getCPSVersion() == 1 && !m_cartridge->isCPS1QSound()) {
        YM2151Shutdown();
        MSM6295Exit(0);
    } else {
        QscExit();
    }
}

void Audio::setSoundCPU(SoundCPU* soundCpu) {
    m_soundCpu = soundCpu;
    s_ym2151SoundCpu = soundCpu;
}

void Audio::setMemory(Memory* memory) {
    m_memory = memory;
}

void Audio::setCartridge(Cartridge* cartridge) {
    m_cartridge = cartridge;
}

void Audio::reset() {
    // Reset sound chips
    if (m_cartridge->getCPSVersion() == 1 && !m_cartridge->isCPS1QSound()) {
        // Initialize YM2151
        YM2151Init(1, 0, 3579540, m_sampleRate, nullptr);
        YM2151SetIrqHandler(0, ym2151IrqHandler);
        
        // Initialize MSM6295
        MSM6295Init(0, 7576, false, m_sampleRate, 0);
        MSM6295SetRoute(0, 1, BURN_SND_ROUTE_BOTH);

        YM2151ResetChip(0);
        MSM6295Reset(0);

        m_ym2151RegSelect = 0;
    } else {
        // Initialize QSound (for both CPS2 and CPS1 QSound games)
        QscInit(m_sampleRate);
        QscSetRoute(BURN_SND_QSND_OUTPUT_1, 1.0, BURN_SND_ROUTE_LEFT);
        QscSetRoute(BURN_SND_QSND_OUTPUT_2, 1.0, BURN_SND_ROUTE_RIGHT);
        QscReset();
    }

    // Cache timing constants
    if (m_cartridge->getCPSVersion() == 2) {
        m_soundCPUCyclesPerFrame = ::cps2::SOUND_CPU_CYCLES_PER_FRAME;
        m_soundCPUFrequency = ::cps2::SOUND_CPU_FREQUENCY;
    } else if (m_cartridge->isCPS1QSound()) {
        m_soundCPUCyclesPerFrame = ::cps1qs::SOUND_CPU_CYCLES_PER_FRAME;
        m_soundCPUFrequency = ::cps1qs::SOUND_CPU_FREQUENCY;
    } else {
        m_soundCPUCyclesPerFrame = ::cps1::SOUND_CPU_CYCLES_PER_FRAME;
        m_soundCPUFrequency = ::cps1::SOUND_CPU_FREQUENCY;
    }
    m_soundCyclesPerSample = m_soundCPUFrequency / m_sampleRate;

    // Set ROM data
    setROMData();
}

void Audio::setROMData() {
    // Get sound sample data from cartridge
    const u8* sampleData = m_cartridge->getSoundSample();
    u32 sampleSize = m_cartridge->getSoundSampleSize();

    if (sampleData && sampleSize > 0) {
        if (m_cartridge->getCPSVersion() == 1 && !m_cartridge->isCPS1QSound()) {
            // CPS1: MSM6295 samples
            s32 endAddr = static_cast<s32>(sampleSize - 1);
            MSM6295SetBank(0, const_cast<u8*>(sampleData), 0, endAddr);
        } else {
            // QSound samples (CPS1 QSound games and CPS2)
            QscSetSampleROM(const_cast<u8*>(sampleData), sampleSize);
        }
    }
}

void Audio::setSampleRate(u32 sampleRate) {
    m_sampleRate = sampleRate;

    if (m_cartridge->getCPSVersion() == 1 && !m_cartridge->isCPS1QSound()) {
        YM2151SetSampleRate(0, sampleRate);
        MSM6295SetSamplerate(0, 7576, static_cast<s32>(sampleRate));
    } else {
        QscSetSampleRate(sampleRate);
    }
}

void Audio::setVolume(float volume) {
    m_volume = volume;
}

u8 Audio::readPort(u16 port) {
    if (m_cartridge->getCPSVersion() == 1 && !m_cartridge->isCPS1QSound()) {
        // CPS1: YM2151 and MSM6295
        // YM2151 status register (port 0x01)
        if (port == 0x01) {
            return static_cast<u8>(YM2151ReadStatus(0));
        }
        // MSM6295 status register (port 0x02)
        if (port == 0x02) {
            return static_cast<u8>(MSM6295Read(0));
        }
    } else if (m_cartridge->isCPS1QSound()) {
        // QSound status register (port 0x07)
        if (port == 0x07) {
            return readQSound();
        }
    }

    return 0xFF;
}

u8 Audio::readQSound() {
    return QscRead();
}

void Audio::writePort(u16 port, u8 value) {
    if (m_cartridge->getCPSVersion() == 1 && !m_cartridge->isCPS1QSound()) {
        // CPS1: YM2151 and MSM6295
        if (port == 0x00) {
            m_ym2151RegSelect = value;
            return;
        }
        if (port == 0x01) {
            YM2151WriteReg(0, m_ym2151RegSelect, value);
            return;
        }
        if (port == 0x02) {
            MSM6295Write(0, value);
            return;
        }
    }
}

void Audio::writeQSound(u16 port, u16 value) {
    QscWrite(port, value);
}

void Audio::renderOneSample(u32 index) {
    if (m_cartridge->getCPSVersion() == 1 && !m_cartridge->isCPS1QSound()) {
        // CPS1: YM2151 (1 sample)
        s16* ymBufs[2] = { &m_ym2151Left[index], &m_ym2151Right[index] };
        YM2151UpdateOne(0, ymBufs, 1);

        // CPS1: MSM6295 (1 sample, interleaved L,R)
        MSM6295Render(0, &m_msm6295Buf[index * 2], 1);
    } else {
        // QSound (CPS2 / CPS1 QSound)
        QscUpdate(1);
        m_qsoundLeft[index] = QscGetLeftSample();
        m_qsoundRight[index] = QscGetRightSample();
    }
}

void Audio::runSoundCPUTo(s32 targetZ80Cycle) {
    s32 remaining = targetZ80Cycle - static_cast<s32>(m_soundCpu->frameCycles());
    if (remaining > 0) {
        m_soundCpu->step(static_cast<u32>(remaining));
    }
}

void Audio::endFrame(double gameSpeed) {
    if (!m_audioDevice) return;

    // Calculate samples for this frame
    u32 totalSamples = m_soundCPUCyclesPerFrame / m_soundCyclesPerSample;
    if (totalSamples > AUDIO_BUFFER_SIZE) totalSamples = AUDIO_BUFFER_SIZE;
    if (totalSamples == 0) return;

    // Interleave: run Z80 for one sample's worth of cycles, then render that sample
    s32 targetCycle = 0;
    for (u32 i = 0; i < totalSamples; i++) {
        targetCycle += m_soundCyclesPerSample;
        runSoundCPUTo(targetCycle);
        renderOneSample(i);
    }

    // Compute output sample count (stretch for gameSpeed < 1)
    u32 outputSamples = (gameSpeed > 0.0)
        ? static_cast<u32>(totalSamples / gameSpeed)
        : totalSamples;

    u32 mixPos = 0;

    for (u32 i = 0; i < outputSamples; i++) {
        u32 src = (gameSpeed > 0.0 && gameSpeed != 1.0)
            ? static_cast<u32>(i * gameSpeed)
            : i;
        if (src >= totalSamples) src = totalSamples - 1;

        float left, right;

        if (m_cartridge->getCPSVersion() == 1 && !m_cartridge->isCPS1QSound()) {
            // CPS1: mix YM2151 + MSM6295
            float ymL = m_ym2151Left[src] / 32768.0f;
            float ymR = m_ym2151Right[src] / 32768.0f;
            float msmL = m_msm6295Buf[src * 2] / 32768.0f;
            float msmR = m_msm6295Buf[src * 2 + 1] / 32768.0f;
            left  = ymL * 0.35f + msmL * 0.30f;
            right = ymR * 0.35f + msmR * 0.30f;
        } else {
            // QSound
            left  = m_qsoundLeft[src] / 32768.0f;
            right = m_qsoundRight[src] / 32768.0f;
        }

        left  = std::clamp(left * m_volume, -1.0f, 1.0f);
        right = std::clamp(right * m_volume, -1.0f, 1.0f);

        m_mixBuffer[mixPos++] = left;
        m_mixBuffer[mixPos++] = right;

        // Flush when mix buffer is full
        if (mixPos >= m_mixBuffer.size()) {
            m_audioDevice->writeSamples(m_mixBuffer.data(), mixPos * sizeof(float));
            mixPos = 0;
        }
    }

    if (mixPos > 0) {
        m_audioDevice->writeSamples(m_mixBuffer.data(), mixPos * sizeof(float));
    }
}

void Audio::saveState(Buffer* buf) {
    buffer_write(buf, &m_sampleRate, sizeof(m_sampleRate));
    buffer_write(buf, &m_volume, sizeof(m_volume));
    buffer_write(buf, &m_ym2151RegSelect, sizeof(m_ym2151RegSelect));

    if (m_cartridge->getCPSVersion() == 1 && !m_cartridge->isCPS1QSound()) {
        YM2151SaveContext(buf);
        MSM6295SaveContext(buf);
    } else {
        QscSaveContext(buf);
    }
}

void Audio::loadState(Buffer* buf) {
    buffer_read(buf, &m_sampleRate, sizeof(m_sampleRate));
    buffer_read(buf, &m_volume, sizeof(m_volume));
    buffer_read(buf, &m_ym2151RegSelect, sizeof(m_ym2151RegSelect));

    if (m_cartridge->getCPSVersion() == 1 && !m_cartridge->isCPS1QSound()) {
        YM2151LoadContext(buf);
        MSM6295LoadContext(buf);
    } else {
        QscLoadContext(buf);
    }
}

} // namespace cps