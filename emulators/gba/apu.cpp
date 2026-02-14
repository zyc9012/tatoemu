#include "apu.h"
#include "memory.h"
#include "timer.h"
#include "dma.h"
#include <cstring>
#include <algorithm>

namespace gba {

// GBA sound IO register offsets (relative to 0x04000000)
namespace SoundIO {
    constexpr u32 SOUND1CNT_L = 0x060; // Channel 1 Sweep
    constexpr u32 SOUND1CNT_H = 0x062; // Channel 1 Duty/Len/Env
    constexpr u32 SOUND1CNT_X = 0x064; // Channel 1 Freq/Control
    constexpr u32 SOUND2CNT_L = 0x068; // Channel 2 Duty/Len/Env
    constexpr u32 SOUND2CNT_H = 0x06C; // Channel 2 Freq/Control
    constexpr u32 SOUND3CNT_L = 0x070; // Channel 3 Stop/Wave RAM select
    constexpr u32 SOUND3CNT_H = 0x072; // Channel 3 Length/Volume
    constexpr u32 SOUND3CNT_X = 0x074; // Channel 3 Freq/Control
    constexpr u32 SOUND4CNT_L = 0x078; // Channel 4 Len/Env
    constexpr u32 SOUND4CNT_H = 0x07C; // Channel 4 Freq/Control
    constexpr u32 SOUNDCNT_L  = 0x080; // Control Stereo/Volume/Enable
    constexpr u32 SOUNDCNT_H  = 0x082; // Control Mixing/DMA Control
    constexpr u32 SOUNDCNT_X  = 0x084; // Control Sound on/off
    constexpr u32 SOUNDBIAS   = 0x088; // Sound PWM Control
    constexpr u32 WAVE_RAM0   = 0x090; // Channel 3 Wave Pattern RAM
    constexpr u32 WAVE_RAM1   = 0x094;
    constexpr u32 WAVE_RAM2   = 0x098;
    constexpr u32 WAVE_RAM3   = 0x09C;
    constexpr u32 FIFO_A      = 0x0A0; // Channel A FIFO
    constexpr u32 FIFO_B      = 0x0A4; // Channel B FIFO
}

constexpr u8 APU::DUTY_PATTERNS[4][8];

APU::APU()
    : m_psgVolumeRight(7)
    , m_psgVolumeLeft(7)
    , m_psgEnableRight(0xF)
    , m_psgEnableLeft(0xF)
    , m_psgMasterVolume(2)
    , m_masterEnable(false)
    , m_soundBias(0x200)
    , m_frameSequencerTimer(0)
    , m_frameSequencerStep(0)
    , m_sampleTimer(0)
    , m_sampleRate(44100)
    , m_volume(1.0f)
    , m_capacitorLeft(0.0f)
    , m_capacitorRight(0.0f) {
}

APU::~APU() {}

void APU::reset() {
    m_square1.reset();
    m_square2.reset();
    m_wave.reset();
    m_noise.reset();
    m_fifoA.reset();
    m_fifoB.reset();

    m_psgVolumeRight = 7;
    m_psgVolumeLeft = 7;
    m_psgEnableRight = 0xF;
    m_psgEnableLeft = 0xF;
    m_psgMasterVolume = 2;
    m_masterEnable = true;
    m_soundBias = 0x200;

    m_frameSequencerTimer = 0;
    m_frameSequencerStep = 0;
    m_sampleTimer = 0;
    m_capacitorLeft = 0.0f;
    m_capacitorRight = 0.0f;
}

// ---------------------------------------------------------------
// step() — called every CPU cycle batch
// ---------------------------------------------------------------
void APU::step(u32 cycles, double gameSpeed) {
    if (!m_masterEnable) {
        // Still generate silence to keep the audio buffer filled
        m_sampleTimer += cycles;
        u32 cyclesPerSample = (CPU_FREQUENCY * gameSpeed) / m_sampleRate;
        while (m_sampleTimer >= cyclesPerSample) {
            m_sampleTimer -= cyclesPerSample;
            // Push silence
            if (m_audioDevice) {
                float silence[2] = {0.0f, 0.0f};
                m_audioDevice->writeSamples(silence, sizeof(silence));
            }
        }
        return;
    }

    // Frame sequencer at 512 Hz (every 32768 CPU cycles at 16.78 MHz)
    // GBA frame sequencer period = CPU_FREQUENCY / 512 = 32768
    static constexpr u32 FRAME_SEQ_PERIOD = 32768;

    for (u32 i = 0; i < cycles; i++) {
        // Clock frame sequencer
        m_frameSequencerTimer++;
        if (m_frameSequencerTimer >= FRAME_SEQ_PERIOD) {
            m_frameSequencerTimer -= FRAME_SEQ_PERIOD;
            clockFrameSequencer();
        }

        // Clock square channel 1
        if (m_square1.enabled && m_square1.frequencyTimer > 0) {
            m_square1.frequencyTimer--;
            if (m_square1.frequencyTimer == 0) {
                m_square1.frequencyTimer = m_square1.getFrequencyTimerPeriod();
                m_square1.dutyPosition = (m_square1.dutyPosition + 1) & 7;
            }
        }

        // Clock square channel 2
        if (m_square2.enabled && m_square2.frequencyTimer > 0) {
            m_square2.frequencyTimer--;
            if (m_square2.frequencyTimer == 0) {
                m_square2.frequencyTimer = m_square2.getFrequencyTimerPeriod();
                m_square2.dutyPosition = (m_square2.dutyPosition + 1) & 7;
            }
        }

        // Clock wave channel
        if (m_wave.enabled && m_wave.frequencyTimer > 0) {
            m_wave.frequencyTimer--;
            if (m_wave.frequencyTimer == 0) {
                m_wave.frequencyTimer = (2048 - m_wave.frequency) * 2;
                m_wave.wavePosition = (m_wave.wavePosition + 1) & 31;
            }
        }

        // Clock noise channel
        if (m_noise.enabled && m_noise.frequencyTimer > 0) {
            m_noise.frequencyTimer--;
            if (m_noise.frequencyTimer == 0) {
                m_noise.frequencyTimer = m_noise.getFrequencyPeriod();
                u16 bit = ~(m_noise.lfsr ^ (m_noise.lfsr >> 1)) & 1;
                m_noise.lfsr &= ~0x8000;
                m_noise.lfsr |= (bit << 15);
                if (m_noise.widthMode) {
                    m_noise.lfsr &= ~0x0080;
                    m_noise.lfsr |= (bit << 7);
                }
                m_noise.lfsr >>= 1;
            }
        }

        // Generate output sample at the configured sample rate
        generateSample(gameSpeed);
    }
}

// ---------------------------------------------------------------
// Frame sequencer (512 Hz)
// ---------------------------------------------------------------
void APU::clockFrameSequencer() {
    switch (m_frameSequencerStep) {
        case 0: case 4:
            m_square1.clockLength();
            m_square2.clockLength();
            m_wave.clockLength();
            m_noise.clockLength();
            break;
        case 2: case 6:
            m_square1.clockLength();
            m_square2.clockLength();
            m_wave.clockLength();
            m_noise.clockLength();
            m_square1.clockSweep();
            break;
        case 7:
            m_square1.clockEnvelope();
            m_square2.clockEnvelope();
            m_noise.clockEnvelope();
            break;
        default:
            break;
    }
    m_frameSequencerStep = (m_frameSequencerStep + 1) & 7;
}

// ---------------------------------------------------------------
// Sample generation and mixing
// ---------------------------------------------------------------
void APU::generateSample(double gameSpeed) {
    m_sampleTimer++;
    u32 cyclesPerSample = (CPU_FREQUENCY * gameSpeed) / m_sampleRate;

    if (m_sampleTimer < cyclesPerSample) return;
    m_sampleTimer -= cyclesPerSample;

    // --- PSG channel outputs (0..15 range) ---
    s16 ch1 = m_square1.getOutput();
    s16 ch2 = m_square2.getOutput();
    s16 ch3 = m_wave.getOutput();
    s16 ch4 = m_noise.getOutput();

    // Mix PSG left/right according to SOUNDCNT_L panning
    float psgLeft = 0.0f;
    float psgRight = 0.0f;

    if (m_psgEnableLeft & 0x1) psgLeft  += ch1;
    if (m_psgEnableLeft & 0x2) psgLeft  += ch2;
    if (m_psgEnableLeft & 0x4) psgLeft  += ch3;
    if (m_psgEnableLeft & 0x8) psgLeft  += ch4;

    if (m_psgEnableRight & 0x1) psgRight += ch1;
    if (m_psgEnableRight & 0x2) psgRight += ch2;
    if (m_psgEnableRight & 0x4) psgRight += ch3;
    if (m_psgEnableRight & 0x8) psgRight += ch4;

    // Apply PSG master volume per side (0-7 → multiply by (vol+1)/8)
    psgLeft  *= (m_psgVolumeLeft  + 1) / 8.0f;
    psgRight *= (m_psgVolumeRight + 1) / 8.0f;

    // Apply PSG master volume ratio from SOUNDCNT_H bits 0-1
    // 0=25%, 1=50%, 2=100%
    float psgRatio;
    switch (m_psgMasterVolume) {
        case 0:  psgRatio = 0.25f; break;
        case 1:  psgRatio = 0.50f; break;
        default: psgRatio = 1.00f; break;
    }
    psgLeft  *= psgRatio;
    psgRight *= psgRatio;

    // Normalize PSG: max per channel is 15, 4 channels, so max sum = 60
    psgLeft  /= 60.0f;
    psgRight /= 60.0f;

    // --- DMA / FIFO channel outputs (-128..+127 range) ---
    float fifoAOut = static_cast<float>(m_fifoA.currentSample) / 128.0f;
    float fifoBOut = static_cast<float>(m_fifoB.currentSample) / 128.0f;

    // Apply FIFO volume (50% or 100%)
    if (!m_fifoA.fullVolume) fifoAOut *= 0.5f;
    if (!m_fifoB.fullVolume) fifoBOut *= 0.5f;

    // Mix everything together
    float left  = psgLeft;
    float right = psgRight;

    if (m_fifoA.enableLeft)  left  += fifoAOut;
    if (m_fifoA.enableRight) right += fifoAOut;
    if (m_fifoB.enableLeft)  left  += fifoBOut;
    if (m_fifoB.enableRight) right += fifoBOut;

    // Clamp to -1..1 (PSG ~1.0 + 2 FIFOs ~1.0 each = up to ~3.0)
    left  /= 3.0f;
    right /= 3.0f;

    // High-pass filter to remove DC offset
    const float hpStrength = 0.999f;
    float leftFiltered  = left  - m_capacitorLeft;
    m_capacitorLeft     = left  - leftFiltered * hpStrength;
    float rightFiltered = right - m_capacitorRight;
    m_capacitorRight    = right - rightFiltered * hpStrength;

    // Apply user volume
    leftFiltered  *= m_volume;
    rightFiltered *= m_volume;

    // Clamp
    leftFiltered  = std::clamp(leftFiltered,  -1.0f, 1.0f);
    rightFiltered = std::clamp(rightFiltered, -1.0f, 1.0f);

    float samples[2] = { leftFiltered, rightFiltered };
    if (m_audioDevice) {
        m_audioDevice->writeSamples(samples, sizeof(samples));
    }
}

// ---------------------------------------------------------------
// Timer overflow → FIFO dequeue
// ---------------------------------------------------------------
void APU::onTimerOverflow(int timerChannel) {
    if (!m_masterEnable) return;

    // FIFO A
    if (m_fifoA.timerSelect == timerChannel) {
        m_fifoA.currentSample = m_fifoA.dequeue();
        // Request DMA refill when FIFO has <= 16 samples
        if (m_fifoA.availableSamples() <= 16 && m_dma) {
            // DMA 1 or DMA 2 can refill FIFO A (SPECIAL timing at dest 0x040000A0)
            m_dma->runFIFO(0);
        }
    }

    // FIFO B
    if (m_fifoB.timerSelect == timerChannel) {
        m_fifoB.currentSample = m_fifoB.dequeue();
        if (m_fifoB.availableSamples() <= 16 && m_dma) {
            m_dma->runFIFO(1);
        }
    }
}

// ---------------------------------------------------------------
// IO register read
// ---------------------------------------------------------------
u8 APU::readRegister(u32 offset) const {
    switch (offset) {
        // SOUND1CNT_L — Channel 1 Sweep
        case SoundIO::SOUND1CNT_L:
            return (m_square1.sweepPeriod << 4) |
                   (m_square1.sweepNegate ? 0x08 : 0) |
                   m_square1.sweepShift;
        case SoundIO::SOUND1CNT_L + 1:
            return 0;

        // SOUND1CNT_H — Channel 1 Duty/Len/Env
        case SoundIO::SOUND1CNT_H:
            return (m_square1.dutyCycle << 6); // length is write-only
        case SoundIO::SOUND1CNT_H + 1:
            return (m_square1.volume << 4) |
                   (m_square1.envelopeAddMode ? 0x08 : 0) |
                   m_square1.envelopePeriod;

        // SOUND1CNT_X — Channel 1 Freq/Control
        case SoundIO::SOUND1CNT_X:
            return 0; // frequency is write-only
        case SoundIO::SOUND1CNT_X + 1:
            return (m_square1.lengthEnable ? 0x40 : 0);

        // SOUND2CNT_L — Channel 2 Duty/Len/Env
        case SoundIO::SOUND2CNT_L:
            return (m_square2.dutyCycle << 6);
        case SoundIO::SOUND2CNT_L + 1:
            return (m_square2.volume << 4) |
                   (m_square2.envelopeAddMode ? 0x08 : 0) |
                   m_square2.envelopePeriod;

        // SOUND2CNT_H — Channel 2 Freq/Control
        case SoundIO::SOUND2CNT_H:
            return 0;
        case SoundIO::SOUND2CNT_H + 1:
            return (m_square2.lengthEnable ? 0x40 : 0);

        // SOUND3CNT_L — Channel 3 Stop/Wave RAM select
        case SoundIO::SOUND3CNT_L:
            return (m_wave.dacEnabled ? 0x80 : 0);
        case SoundIO::SOUND3CNT_L + 1:
            return 0;

        // SOUND3CNT_H — Channel 3 Length/Volume
        case SoundIO::SOUND3CNT_H:
            return 0; // length is write-only
        case SoundIO::SOUND3CNT_H + 1:
            return (m_wave.outputLevel << 5) |
                   (m_wave.forceVolume ? 0x80 : 0);

        // SOUND3CNT_X — Channel 3 Freq/Control
        case SoundIO::SOUND3CNT_X:
            return 0;
        case SoundIO::SOUND3CNT_X + 1:
            return (m_wave.lengthEnable ? 0x40 : 0);

        // SOUND4CNT_L — Channel 4 Len/Env
        case SoundIO::SOUND4CNT_L:
            return 0; // length is write-only
        case SoundIO::SOUND4CNT_L + 1:
            return (m_noise.volume << 4) |
                   (m_noise.envelopeAddMode ? 0x08 : 0) |
                   m_noise.envelopePeriod;

        // SOUND4CNT_H — Channel 4 Freq/Control
        case SoundIO::SOUND4CNT_H:
            return (m_noise.clockShift << 4) |
                   (m_noise.widthMode ? 0x08 : 0) |
                   m_noise.divisorCode;
        case SoundIO::SOUND4CNT_H + 1:
            return (m_noise.lengthEnable ? 0x40 : 0);

        // SOUNDCNT_L — Control Stereo/Volume/Enable
        case SoundIO::SOUNDCNT_L:
            return (m_psgVolumeLeft << 4) | m_psgVolumeRight;
        case SoundIO::SOUNDCNT_L + 1:
            return (m_psgEnableLeft << 4) | m_psgEnableRight;

        // SOUNDCNT_H — Control Mixing/DMA
        case SoundIO::SOUNDCNT_H:
            return m_psgMasterVolume |
                   (m_fifoA.fullVolume ? 0x04 : 0) |
                   (m_fifoB.fullVolume ? 0x08 : 0);
        case SoundIO::SOUNDCNT_H + 1:
            return (m_fifoA.enableRight ? 0x01 : 0) |
                   (m_fifoA.enableLeft  ? 0x02 : 0) |
                   (m_fifoA.timerSelect ? 0x04 : 0) |
                   (m_fifoB.enableRight ? 0x10 : 0) |
                   (m_fifoB.enableLeft  ? 0x20 : 0) |
                   (m_fifoB.timerSelect ? 0x40 : 0);

        // SOUNDCNT_X — Sound on/off
        case SoundIO::SOUNDCNT_X:
            return (m_masterEnable ? 0x80 : 0) |
                   (m_noise.enabled   ? 0x08 : 0) |
                   (m_wave.enabled    ? 0x04 : 0) |
                   (m_square2.enabled ? 0x02 : 0) |
                   (m_square1.enabled ? 0x01 : 0);
        case SoundIO::SOUNDCNT_X + 1:
            return 0;

        // SOUNDBIAS
        case SoundIO::SOUNDBIAS:
            return m_soundBias & 0xFF;
        case SoundIO::SOUNDBIAS + 1:
            return (m_soundBias >> 8) & 0xFF;

        // Wave RAM (16 bytes at 0x090-0x09F)
        case SoundIO::WAVE_RAM0:     case SoundIO::WAVE_RAM0 + 1:
        case SoundIO::WAVE_RAM0 + 2: case SoundIO::WAVE_RAM0 + 3:
        case SoundIO::WAVE_RAM1:     case SoundIO::WAVE_RAM1 + 1:
        case SoundIO::WAVE_RAM1 + 2: case SoundIO::WAVE_RAM1 + 3:
        case SoundIO::WAVE_RAM2:     case SoundIO::WAVE_RAM2 + 1:
        case SoundIO::WAVE_RAM2 + 2: case SoundIO::WAVE_RAM2 + 3:
        case SoundIO::WAVE_RAM3:     case SoundIO::WAVE_RAM3 + 1:
        case SoundIO::WAVE_RAM3 + 2: case SoundIO::WAVE_RAM3 + 3:
            return m_wave.waveRAM[offset - SoundIO::WAVE_RAM0];

        default:
            return 0;
    }
}

// ---------------------------------------------------------------
// IO register write (16-bit aligned)
// ---------------------------------------------------------------
void APU::writeRegister(u32 offset, u16 value) {
    if (!m_masterEnable && offset != SoundIO::SOUNDCNT_X) {
        // When sound is off, only SOUNDCNT_X can be written
        return;
    }

    switch (offset) {
        // ---- Channel 1: Square with sweep ----
        case SoundIO::SOUND1CNT_L: // Sweep
            m_square1.sweepPeriod  = (value >> 4) & 7;
            m_square1.sweepNegate  = (value & 0x08) != 0;
            m_square1.sweepShift   = value & 7;
            break;

        case SoundIO::SOUND1CNT_H: // Duty / Length / Envelope
            m_square1.dutyCycle      = (value >> 6) & 3;
            m_square1.lengthCounter  = 64 - (value & 0x3F);
            m_square1.volume         = (value >> 12) & 0xF;
            m_square1.envelopeAddMode = (value & 0x0800) != 0;
            m_square1.envelopePeriod = (value >> 8) & 7;
            m_square1.dacEnabled     = (value & 0xF800) != 0;
            if (!m_square1.dacEnabled) m_square1.enabled = false;
            break;

        case SoundIO::SOUND1CNT_X: // Freq / Control
            m_square1.frequency    = value & 0x7FF;
            m_square1.lengthEnable = (value & 0x4000) != 0;
            if (value & 0x8000) {
                m_square1.trigger(true);
            }
            break;

        // ---- Channel 2: Square ----
        case SoundIO::SOUND2CNT_L: // Duty / Length / Envelope
            m_square2.dutyCycle      = (value >> 6) & 3;
            m_square2.lengthCounter  = 64 - (value & 0x3F);
            m_square2.volume         = (value >> 12) & 0xF;
            m_square2.envelopeAddMode = (value & 0x0800) != 0;
            m_square2.envelopePeriod = (value >> 8) & 7;
            m_square2.dacEnabled     = (value & 0xF800) != 0;
            if (!m_square2.dacEnabled) m_square2.enabled = false;
            break;

        case SoundIO::SOUND2CNT_H: // Freq / Control
            m_square2.frequency    = value & 0x7FF;
            m_square2.lengthEnable = (value & 0x4000) != 0;
            if (value & 0x8000) {
                m_square2.trigger(false);
            }
            break;

        // ---- Channel 3: Wave ----
        case SoundIO::SOUND3CNT_L: // Stop / Wave RAM bank / dimension
            m_wave.dacEnabled = (value & 0x80) != 0;
            if (!m_wave.dacEnabled) m_wave.enabled = false;
            break;

        case SoundIO::SOUND3CNT_H: // Length / Volume
            m_wave.lengthCounter = 256 - (value & 0xFF);
            m_wave.outputLevel   = (value >> 13) & 3;
            m_wave.forceVolume   = (value & 0x8000) != 0;
            break;

        case SoundIO::SOUND3CNT_X: // Freq / Control
            m_wave.frequency    = value & 0x7FF;
            m_wave.lengthEnable = (value & 0x4000) != 0;
            if (value & 0x8000) {
                m_wave.trigger();
            }
            break;

        // ---- Channel 4: Noise ----
        case SoundIO::SOUND4CNT_L: // Length / Envelope
            m_noise.lengthCounter  = 64 - (value & 0x3F);
            m_noise.volume         = (value >> 12) & 0xF;
            m_noise.envelopeAddMode = (value & 0x0800) != 0;
            m_noise.envelopePeriod = (value >> 8) & 7;
            m_noise.dacEnabled     = (value & 0xF800) != 0;
            if (!m_noise.dacEnabled) m_noise.enabled = false;
            break;

        case SoundIO::SOUND4CNT_H: // Freq / Control
            m_noise.clockShift  = (value >> 4) & 0xF;
            m_noise.widthMode   = (value & 0x08) != 0;
            m_noise.divisorCode = value & 7;
            m_noise.lengthEnable = (value & 0x4000) != 0;
            if (value & 0x8000) {
                m_noise.trigger();
            }
            break;

        // ---- Master control ----
        case SoundIO::SOUNDCNT_L: // PSG volume / panning
            m_psgVolumeRight = value & 7;
            m_psgVolumeLeft  = (value >> 4) & 7;
            m_psgEnableRight = (value >> 8) & 0xF;
            m_psgEnableLeft  = (value >> 12) & 0xF;
            break;

        case SoundIO::SOUNDCNT_H: { // DMA Sound control
            m_psgMasterVolume  = value & 3;
            m_fifoA.fullVolume = (value & 0x04) != 0;
            m_fifoB.fullVolume = (value & 0x08) != 0;

            u8 hi = (value >> 8) & 0xFF;
            m_fifoA.enableRight = (hi & 0x01) != 0;
            m_fifoA.enableLeft  = (hi & 0x02) != 0;
            m_fifoA.timerSelect = (hi & 0x04) ? 1 : 0;
            if (hi & 0x08) { // Reset FIFO A
                m_fifoA.readPos = 0;
                m_fifoA.writePos = 0;
                m_fifoA.size = 0;
            }

            m_fifoB.enableRight = (hi & 0x10) != 0;
            m_fifoB.enableLeft  = (hi & 0x20) != 0;
            m_fifoB.timerSelect = (hi & 0x40) ? 1 : 0;
            if (hi & 0x80) { // Reset FIFO B
                m_fifoB.readPos = 0;
                m_fifoB.writePos = 0;
                m_fifoB.size = 0;
            }
            break;
        }

        case SoundIO::SOUNDCNT_X: { // Master enable
            bool wasEnabled = m_masterEnable;
            m_masterEnable = (value & 0x80) != 0;
            if (wasEnabled && !m_masterEnable) {
                // Turning off: reset all PSG channels
                m_square1.reset();
                m_square2.reset();
                m_wave.reset();
                m_noise.reset();
                m_frameSequencerStep = 0;
            }
            break;
        }

        case SoundIO::SOUNDBIAS:
            m_soundBias = value & 0xC3FE; // bits 0, 6-9 reserved
            break;

        // Wave RAM writes (32-bit aligned in hardware, but we handle 16-bit here)
        case SoundIO::WAVE_RAM0:
        case SoundIO::WAVE_RAM0 + 2:
        case SoundIO::WAVE_RAM1:
        case SoundIO::WAVE_RAM1 + 2:
        case SoundIO::WAVE_RAM2:
        case SoundIO::WAVE_RAM2 + 2:
        case SoundIO::WAVE_RAM3:
        case SoundIO::WAVE_RAM3 + 2: {
            int idx = offset - SoundIO::WAVE_RAM0;
            if (idx >= 0 && idx < 16) {
                m_wave.waveRAM[idx]     = value & 0xFF;
                m_wave.waveRAM[idx + 1] = (value >> 8) & 0xFF;
            }
            break;
        }

        // FIFO writes
        case SoundIO::FIFO_A:
        case SoundIO::FIFO_A + 2:
            m_fifoA.write8(value & 0xFF);
            m_fifoA.write8((value >> 8) & 0xFF);
            break;

        case SoundIO::FIFO_B:
        case SoundIO::FIFO_B + 2:
            m_fifoB.write8(value & 0xFF);
            m_fifoB.write8((value >> 8) & 0xFF);
            break;

        default:
            break;
    }
}

// ---------------------------------------------------------------
// PSG Channel implementations
// ---------------------------------------------------------------

// --- SquareChannel ---
void APU::SquareChannel::reset() {
    sweepPeriod = 0; sweepNegate = false; sweepShift = 0;
    sweepTimer = 0; sweepShadow = 0; sweepEnabled = false;
    dutyCycle = 0; lengthCounter = 0;
    volume = 0; envelopeAddMode = false; envelopePeriod = 0;
    envelopeTimer = 0; currentVolume = 0;
    frequency = 0; lengthEnable = false; dacEnabled = false;
    enabled = false; frequencyTimer = 0; dutyPosition = 0;
}

void APU::SquareChannel::trigger(bool hasSweep) {
    enabled = dacEnabled;
    if (!enabled) return;
    if (lengthCounter == 0) lengthCounter = 64;
    frequencyTimer = (2048 - frequency) * 4;
    envelopeTimer = envelopePeriod;
    currentVolume = volume;
    if (hasSweep) {
        sweepShadow = frequency;
        sweepTimer = sweepPeriod ? sweepPeriod : 8;
        sweepEnabled = (sweepPeriod != 0 || sweepShift != 0);
        if (sweepShift != 0) {
            u16 newFreq = sweepShadow >> sweepShift;
            newFreq = sweepNegate ? (sweepShadow - newFreq) : (sweepShadow + newFreq);
            if (newFreq > 2047) enabled = false;
        }
    }
}

void APU::SquareChannel::clockLength() {
    if (lengthEnable && lengthCounter > 0) {
        lengthCounter--;
        if (lengthCounter == 0) enabled = false;
    }
}

void APU::SquareChannel::clockEnvelope() {
    if (envelopePeriod == 0) return;
    if (envelopeTimer > 0) envelopeTimer--;
    if (envelopeTimer == 0) {
        envelopeTimer = envelopePeriod;
        if (envelopeAddMode && currentVolume < 15) currentVolume++;
        else if (!envelopeAddMode && currentVolume > 0) currentVolume--;
    }
}

void APU::SquareChannel::clockSweep() {
    if (sweepTimer > 0) sweepTimer--;
    if (sweepTimer == 0) {
        sweepTimer = sweepPeriod ? sweepPeriod : 8;
        if (sweepEnabled && sweepPeriod != 0) {
            u16 delta = sweepShadow >> sweepShift;
            u16 newFreq = sweepNegate ? (sweepShadow - delta) : (sweepShadow + delta);
            if (newFreq > 2047) {
                enabled = false;
            } else if (sweepShift != 0) {
                sweepShadow = newFreq;
                frequency = newFreq;
                // Second overflow check
                u16 delta2 = sweepShadow >> sweepShift;
                u16 newFreq2 = sweepNegate ? (sweepShadow - delta2) : (sweepShadow + delta2);
                if (newFreq2 > 2047) enabled = false;
            }
        }
    }
}

u32 APU::SquareChannel::getFrequencyTimerPeriod() const {
    return (2048 - frequency) * 4;
}

s16 APU::SquareChannel::getOutput() const {
    if (!enabled || !dacEnabled) return 0;
    u8 dutyBit = APU::DUTY_PATTERNS[dutyCycle][dutyPosition];
    return dutyBit ? currentVolume : 0;
}

// --- WaveChannel ---
void APU::WaveChannel::reset() {
    dacEnabled = false; lengthCounter = 0; outputLevel = 0;
    frequency = 0; lengthEnable = false;
    enabled = false; frequencyTimer = 0; wavePosition = 0;
    forceVolume = false;
    std::fill(waveRAM.begin(), waveRAM.end(), 0);
}

void APU::WaveChannel::trigger() {
    enabled = dacEnabled;
    if (!enabled) return;
    if (lengthCounter == 0) lengthCounter = 256;
    frequencyTimer = (2048 - frequency) * 2;
    wavePosition = 0;
}

void APU::WaveChannel::clockLength() {
    if (lengthEnable && lengthCounter > 0) {
        lengthCounter--;
        if (lengthCounter == 0) enabled = false;
    }
}

s16 APU::WaveChannel::getOutput() const {
    if (!enabled || !dacEnabled) return 0;
    u8 byte = waveRAM[wavePosition / 2];
    u8 sample = (wavePosition & 1) ? (byte & 0x0F) : (byte >> 4);

    if (forceVolume) {
        // GBA extension: force 75% volume (shift right by 25% → multiply by 3/4)
        return (sample * 3) / 4;
    }
    if (outputLevel == 0) return 0;
    return sample >> (outputLevel - 1);
}

// --- NoiseChannel ---
void APU::NoiseChannel::reset() {
    lengthCounter = 0;
    volume = 0; envelopeAddMode = false; envelopePeriod = 0;
    envelopeTimer = 0; currentVolume = 0;
    clockShift = 0; widthMode = false; divisorCode = 0;
    lengthEnable = false; dacEnabled = false;
    enabled = false; frequencyTimer = 0; lfsr = 0;
}

void APU::NoiseChannel::trigger() {
    enabled = dacEnabled;
    if (!enabled) return;
    if (lengthCounter == 0) lengthCounter = 64;
    envelopeTimer = envelopePeriod;
    currentVolume = volume;
    static const u32 divisors[8] = {8, 16, 32, 48, 64, 80, 96, 112};
    frequencyTimer = divisors[divisorCode] << clockShift;
    lfsr = 0;
}

void APU::NoiseChannel::clockLength() {
    if (lengthEnable && lengthCounter > 0) {
        lengthCounter--;
        if (lengthCounter == 0) enabled = false;
    }
}

void APU::NoiseChannel::clockEnvelope() {
    if (envelopePeriod == 0) return;
    if (envelopeTimer > 0) envelopeTimer--;
    if (envelopeTimer == 0) {
        envelopeTimer = envelopePeriod;
        if (envelopeAddMode && currentVolume < 15) currentVolume++;
        else if (!envelopeAddMode && currentVolume > 0) currentVolume--;
    }
}

u32 APU::NoiseChannel::getFrequencyPeriod() const {
    static const u32 divisors[8] = {8, 16, 32, 48, 64, 80, 96, 112};
    return divisors[divisorCode] << clockShift;
}

s16 APU::NoiseChannel::getOutput() const {
    if (!enabled || !dacEnabled) return 0;
    return (lfsr & 1) ? 0 : currentVolume;
}

// --- FIFOChannel ---
void APU::FIFOChannel::reset() {
    std::memset(fifo, 0, sizeof(fifo));
    readPos = 0; writePos = 0; size = 0;
    currentSample = 0; timerSelect = 0;
    enableRight = false; enableLeft = false;
    fullVolume = false;
}

void APU::FIFOChannel::write8(u8 value) {
    if (size < 32) {
        fifo[writePos] = static_cast<s8>(value);
        writePos = (writePos + 1) & 31;
        size++;
    }
}

void APU::FIFOChannel::write32(u32 value) {
    write8(value & 0xFF);
    write8((value >> 8) & 0xFF);
    write8((value >> 16) & 0xFF);
    write8((value >> 24) & 0xFF);
}

s8 APU::FIFOChannel::dequeue() {
    if (size > 0) {
        s8 sample = fifo[readPos];
        readPos = (readPos + 1) & 31;
        size--;
        return sample;
    }
    return 0; // Underflow: return 0
}

// ---------------------------------------------------------------
// Save / Load state
// ---------------------------------------------------------------
void APU::saveState(Buffer* buf) {
    buffer_write(buf, &m_square1, sizeof(m_square1));
    buffer_write(buf, &m_square2, sizeof(m_square2));
    buffer_write(buf, &m_wave, sizeof(m_wave));
    buffer_write(buf, &m_noise, sizeof(m_noise));
    buffer_write(buf, &m_fifoA, sizeof(m_fifoA));
    buffer_write(buf, &m_fifoB, sizeof(m_fifoB));

    buffer_write(buf, &m_psgVolumeRight, sizeof(m_psgVolumeRight));
    buffer_write(buf, &m_psgVolumeLeft, sizeof(m_psgVolumeLeft));
    buffer_write(buf, &m_psgEnableRight, sizeof(m_psgEnableRight));
    buffer_write(buf, &m_psgEnableLeft, sizeof(m_psgEnableLeft));
    buffer_write(buf, &m_psgMasterVolume, sizeof(m_psgMasterVolume));
    buffer_write(buf, &m_masterEnable, sizeof(m_masterEnable));
    buffer_write(buf, &m_soundBias, sizeof(m_soundBias));

    buffer_write(buf, &m_frameSequencerTimer, sizeof(m_frameSequencerTimer));
    buffer_write(buf, &m_frameSequencerStep, sizeof(m_frameSequencerStep));
    buffer_write(buf, &m_sampleTimer, sizeof(m_sampleTimer));
    buffer_write(buf, &m_capacitorLeft, sizeof(m_capacitorLeft));
    buffer_write(buf, &m_capacitorRight, sizeof(m_capacitorRight));
}

void APU::loadState(Buffer* buf) {
    buffer_read(buf, &m_square1, sizeof(m_square1));
    buffer_read(buf, &m_square2, sizeof(m_square2));
    buffer_read(buf, &m_wave, sizeof(m_wave));
    buffer_read(buf, &m_noise, sizeof(m_noise));
    buffer_read(buf, &m_fifoA, sizeof(m_fifoA));
    buffer_read(buf, &m_fifoB, sizeof(m_fifoB));

    buffer_read(buf, &m_psgVolumeRight, sizeof(m_psgVolumeRight));
    buffer_read(buf, &m_psgVolumeLeft, sizeof(m_psgVolumeLeft));
    buffer_read(buf, &m_psgEnableRight, sizeof(m_psgEnableRight));
    buffer_read(buf, &m_psgEnableLeft, sizeof(m_psgEnableLeft));
    buffer_read(buf, &m_psgMasterVolume, sizeof(m_psgMasterVolume));
    buffer_read(buf, &m_masterEnable, sizeof(m_masterEnable));
    buffer_read(buf, &m_soundBias, sizeof(m_soundBias));

    buffer_read(buf, &m_frameSequencerTimer, sizeof(m_frameSequencerTimer));
    buffer_read(buf, &m_frameSequencerStep, sizeof(m_frameSequencerStep));
    buffer_read(buf, &m_sampleTimer, sizeof(m_sampleTimer));
    buffer_read(buf, &m_capacitorLeft, sizeof(m_capacitorLeft));
    buffer_read(buf, &m_capacitorRight, sizeof(m_capacitorRight));
}

} // namespace gba
