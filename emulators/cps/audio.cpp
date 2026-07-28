#include "audio.h"
#include "cpu.h"
#include "sound_cpu.h"
#include "memory.h"
#include "cartridge.h"
#include "../types.h"
#include <cstring>
#include <cmath>
#include <algorithm>
#include "consts.h"

namespace cps {

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
    , m_soundCyclesPerSample(0)
    , m_mixPos(0) {
}

Audio::~Audio() {
}

void Audio::setSoundCPU(SoundCPU* soundCpu) {
    m_soundCpu = soundCpu;
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
        m_ym2151.init(3579540, m_sampleRate);
        m_ym2151.setIrqHandler([this](bool asserted) { m_soundCpu->irq(asserted); });

        // Initialize MSM6295
        m_msm6295.init(7576, m_sampleRate);
        m_msm6295.setVolume(1.0);

        m_ym2151RegSelect = 0;
    } else {
        // Initialize QSound (for both CPS2 and CPS1 QSound games)
        m_qsound.init(m_sampleRate);
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
            m_msm6295.setBank(sampleData, 0, sampleSize - 1);
        } else {
            // QSound samples (CPS1 QSound games and CPS2)
            m_qsound.setSampleROM(sampleData, sampleSize);
        }
    }
}

void Audio::setSampleRate(u32 sampleRate) {
    m_sampleRate = sampleRate;

    if (m_cartridge->getCPSVersion() == 1 && !m_cartridge->isCPS1QSound()) {
        m_ym2151.setSampleRate(sampleRate);
        m_msm6295.setSampleRate(7576, sampleRate);
    } else {
        m_qsound.setSampleRate(sampleRate);
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
            return m_ym2151.readStatus();
        }
        // MSM6295 status register (port 0x02)
        if (port == 0x02) {
            return m_msm6295.readStatus();
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
    return m_qsound.readStatus();
}

void Audio::writePort(u16 port, u8 value) {
    if (m_cartridge->getCPSVersion() == 1 && !m_cartridge->isCPS1QSound()) {
        // CPS1: YM2151 and MSM6295
        if (port == 0x00) {
            m_ym2151RegSelect = value;
            return;
        }
        if (port == 0x01) {
            m_ym2151.write(m_ym2151RegSelect, value);
            return;
        }
        if (port == 0x02) {
            m_msm6295.write(value);
            return;
        }
    }
}

void Audio::writeQSound(u16 port, u16 value) {
    m_qsound.write(static_cast<u8>(port), value);
}

void Audio::renderAndMixOneSample() {
    float left, right;

    if (m_cartridge->getCPSVersion() == 1 && !m_cartridge->isCPS1QSound()) {
        // CPS1: YM2151 (1 sample into local buffers)
        s16 ymL, ymR;
        m_ym2151.update(&ymL, &ymR, 1);

        // CPS1: MSM6295 (1 mono sample)
        s16 msm;
        m_msm6295.render(&msm, 1);

        left  = (ymL * 0.35f + msm * 0.30f) / 32768.0f;
        right = (ymR * 0.35f + msm * 0.30f) / 32768.0f;
    } else {
        // QSound (CPS2 / CPS1 QSound)
        m_qsound.update(1);
        left  = m_qsound.getLeftSample() / 32768.0f;
        right = m_qsound.getRightSample() / 32768.0f;
    }

    m_mixBuffer[m_mixPos++] = std::clamp(left * m_volume, -1.0f, 1.0f);
    m_mixBuffer[m_mixPos++] = std::clamp(right * m_volume, -1.0f, 1.0f);

    if (m_mixPos >= MIX_BUFFER_SIZE) {
        flushMixBuffer();
    }
}

void Audio::flushMixBuffer() {
    if (m_mixPos > 0) {
        m_audioDevice->writeSamples(m_mixBuffer.data(), m_mixPos * sizeof(float));
        m_mixPos = 0;
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

    // Adjust cycle stride for game speed to preserve pitch
    u32 cyclesPerSample = (gameSpeed > 0.0)
        ? static_cast<u32>(m_soundCyclesPerSample * gameSpeed)
        : m_soundCyclesPerSample;
    if (cyclesPerSample == 0) cyclesPerSample = 1;

    u32 totalSamples = m_soundCPUCyclesPerFrame / cyclesPerSample;
    if (totalSamples == 0) return;

    // Interleave: run Z80 for one sample's worth of cycles, render & mix
    m_mixPos = 0;
    s32 targetCycle = 0;
    for (u32 i = 0; i < totalSamples; i++) {
        targetCycle += cyclesPerSample;
        runSoundCPUTo(targetCycle);
        renderAndMixOneSample();
    }

    // Ensure Z80 has reached end of frame
    runSoundCPUTo(static_cast<s32>(m_soundCPUCyclesPerFrame));

    flushMixBuffer();
}

void Audio::saveState(Buffer* buf) {
    buffer_write(buf, &m_sampleRate, sizeof(m_sampleRate));
    buffer_write(buf, &m_volume, sizeof(m_volume));
    buffer_write(buf, &m_ym2151RegSelect, sizeof(m_ym2151RegSelect));

    if (m_cartridge->getCPSVersion() == 1 && !m_cartridge->isCPS1QSound()) {
        m_ym2151.saveState(buf);
        m_msm6295.saveState(buf);
    } else {
        m_qsound.saveState(buf);
    }
}

void Audio::loadState(Buffer* buf) {
    buffer_read(buf, &m_sampleRate, sizeof(m_sampleRate));
    buffer_read(buf, &m_volume, sizeof(m_volume));
    buffer_read(buf, &m_ym2151RegSelect, sizeof(m_ym2151RegSelect));

    if (m_cartridge->getCPSVersion() == 1 && !m_cartridge->isCPS1QSound()) {
        m_ym2151.loadState(buf);
        m_msm6295.loadState(buf);
    } else {
        m_qsound.loadState(buf);
    }
}

} // namespace cps