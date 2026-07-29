#include "apu.h"
#include "cpu.h"
#include "mmu.h"
#include "consts.h"
#include <cstring>
#include <cmath>
#include <algorithm>

namespace gb {

// Duty cycle patterns
constexpr u8 APU::DUTY_PATTERNS[4][8];

APU::APU()
    : m_cpu(nullptr)
    , m_mmu(nullptr)
    , m_audioDevice(nullptr)
    , m_leftVolume(7)
    , m_rightVolume(7)
    , m_leftVinEnable(false)
    , m_rightVinEnable(false)
    , m_leftEnable(0xFF)
    , m_rightEnable(0xFF)
    , m_enabled(false)
    , m_frameSequencerTimer(0)
    , m_frameSequencerStep(0)
    , m_sampleTimer(0)
    , m_cycleAccumulator(0)
    , m_capacitorLeft(0.0f)
    , m_capacitorRight(0.0f) {
}

APU::~APU() {
}

void APU::setCPU(CPU* cpu) {
    m_cpu = cpu;
}

void APU::setMMU(MMU* mmu) {
    m_mmu = mmu;
}

void APU::setAudioDevice(AudioDevice* device) {
    m_audioDevice = device;
}

void APU::reset() {
    m_square1.reset();
    m_square2.reset();
    m_wave.reset();
    m_noise.reset();
    
    m_leftVolume = 7;
    m_rightVolume = 7;
    m_leftVinEnable = false;
    m_rightVinEnable = false;
    m_leftEnable = 0xFF;
    m_rightEnable = 0xFF;
    m_enabled = true; // APU starts enabled after bootrom
    
    m_frameSequencerTimer = 0;
    m_frameSequencerStep = 0;
    m_sampleTimer = 0;
    m_cycleAccumulator = 0;
    
    m_capacitorLeft = 0.0f;
    m_capacitorRight = 0.0f;
}

void APU::step(u32 cycles, double playbackSpeed = 1.0) {
    // The APU always runs at normal speed (4.194304 MHz), even in double speed mode
    // In double speed mode, we receive 2x the cycles, but we should only process
    // half of them for APU timing
    u32 apuCycles = cycles;
    if (m_mmu->isDoubleSpeed()) {
        // Accumulate cycles to handle odd counts properly
        m_cycleAccumulator += cycles;
        apuCycles = m_cycleAccumulator / 2;
        m_cycleAccumulator %= 2;
    }
    
    if (!m_enabled) {
        // When APU is disabled, still advance sample timer to stay in sync
        m_sampleTimer += apuCycles;
        u32 cyclesPerSample = CLOCK_SPEED / m_sampleRate;
        while (m_sampleTimer >= cyclesPerSample) {
            m_sampleTimer -= cyclesPerSample;
        }
        return;
    }
    
    u32 cyclesPerSample = std::max(1u, (u32)(CLOCK_SPEED * playbackSpeed / m_sampleRate));
    u32 remaining = apuCycles;
    
    while (remaining > 0) {
        // Find minimum cycles until next event
        u32 delta = remaining;
        
        // Frame sequencer: fires every 8192 APU cycles
        u32 fsRemain = 8192 - m_frameSequencerTimer;
        if (fsRemain < delta) delta = fsRemain;
        
        // Sample generation
        u32 sampleRemain = (m_sampleTimer < cyclesPerSample)
            ? (cyclesPerSample - m_sampleTimer) : 1;
        if (sampleRemain < delta) delta = sampleRemain;
        
        // Channel frequency timers
        if (m_square1.enabled && m_square1.frequencyTimer > 0
            && (u32)m_square1.frequencyTimer < delta)
            delta = m_square1.frequencyTimer;
        if (m_square2.enabled && m_square2.frequencyTimer > 0
            && (u32)m_square2.frequencyTimer < delta)
            delta = m_square2.frequencyTimer;
        if (m_wave.enabled && m_wave.frequencyTimer > 0
            && (u32)m_wave.frequencyTimer < delta)
            delta = m_wave.frequencyTimer;
        if (m_noise.enabled && m_noise.frequencyTimer > 0
            && (u32)m_noise.frequencyTimer < delta)
            delta = m_noise.frequencyTimer;
        
        if (delta == 0) delta = 1;
        
        // Advance all timers
        remaining -= delta;
        m_frameSequencerTimer += delta;
        m_sampleTimer += delta;
        
        // Process frame sequencer
        if (m_frameSequencerTimer >= 8192) {
            m_frameSequencerTimer -= 8192;
            clockFrameSequencer();
        }
        
        // Process channel frequency timers
        if (m_square1.enabled && m_square1.frequencyTimer > 0) {
            m_square1.frequencyTimer -= delta;
            if (m_square1.frequencyTimer == 0) {
                m_square1.frequencyTimer = m_square1.getFrequencyTimerPeriod();
                m_square1.dutyPosition = (m_square1.dutyPosition + 1) & 7;
            }
        }
        
        if (m_square2.enabled && m_square2.frequencyTimer > 0) {
            m_square2.frequencyTimer -= delta;
            if (m_square2.frequencyTimer == 0) {
                m_square2.frequencyTimer = m_square2.getFrequencyTimerPeriod();
                m_square2.dutyPosition = (m_square2.dutyPosition + 1) & 7;
            }
        }
        
        if (m_wave.enabled && m_wave.frequencyTimer > 0) {
            m_wave.frequencyTimer -= delta;
            if (m_wave.frequencyTimer == 0) {
                m_wave.frequencyTimer = (2048 - m_wave.frequency) * 2;
                m_wave.wavePosition = (m_wave.wavePosition + 1) & 31;
            }
        }
        
        if (m_noise.enabled && m_noise.frequencyTimer > 0) {
            m_noise.frequencyTimer -= delta;
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
        
        // Generate sample if ready
        if (m_sampleTimer >= cyclesPerSample) {
            m_sampleTimer -= cyclesPerSample;
            outputSample();
        }
    }
}

void APU::clockFrameSequencer() {
    // Frame sequencer runs at 512 Hz
    // Step 0: Length
    // Step 1: Nothing
    // Step 2: Length & Sweep
    // Step 3: Nothing
    // Step 4: Length
    // Step 5: Nothing
    // Step 6: Length & Sweep
    // Step 7: Volume envelope
    
    switch (m_frameSequencerStep) {
        case 0:
        case 2:
        case 4:
        case 6:
            // Clock length counters
            m_square1.clockLength();
            m_square2.clockLength();
            m_wave.clockLength();
            m_noise.clockLength();
            
            // Clock sweep on steps 2 and 6
            if (m_frameSequencerStep == 2 || m_frameSequencerStep == 6) {
                m_square1.clockSweep();
            }
            break;
        
        case 7:
            // Clock volume envelopes
            m_square1.clockEnvelope();
            m_square2.clockEnvelope();
            m_noise.clockEnvelope();
            break;
    }
    
    m_frameSequencerStep = (m_frameSequencerStep + 1) & 7;
    updateNR52();
}

void APU::outputSample() {
    // Get channel outputs
    s16 ch1 = m_square1.getOutput() - DAC_BIAS;
    s16 ch2 = m_square2.getOutput() - DAC_BIAS;
    s16 ch3 = m_wave.getOutput() - DAC_BIAS;
    s16 ch4 = m_noise.getOutput() - DAC_BIAS;
    
    // Mix left channel (NR51 bits 4-7)
    float leftMix = 0.0f;
    if (m_leftEnable & 0x01) leftMix += ch1;
    if (m_leftEnable & 0x02) leftMix += ch2;
    if (m_leftEnable & 0x04) leftMix += ch3;
    if (m_leftEnable & 0x08) leftMix += ch4;
    leftMix /= 4.0f;
    
    // Mix right channel (NR51 bits 0-3)
    float rightMix = 0.0f;
    if (m_rightEnable & 0x01) rightMix += ch1;
    if (m_rightEnable & 0x02) rightMix += ch2;
    if (m_rightEnable & 0x04) rightMix += ch3;
    if (m_rightEnable & 0x08) rightMix += ch4;
    rightMix /= 4.0f;
    
    // Apply master volume (NR50)
    leftMix *= (m_leftVolume + 1) / 8.0f;
    rightMix *= (m_rightVolume + 1) / 8.0f;
    
    // Normalize to -1.0 to 1.0 range
    leftMix /= 15.0f;
    rightMix /= 15.0f;
    
    // Apply high-pass filter (capacitor simulation)
    const float highPassStrength = 0.999f;
    float leftFiltered = leftMix - m_capacitorLeft;
    m_capacitorLeft = leftMix - leftFiltered * highPassStrength;
    float rightFiltered = rightMix - m_capacitorRight;
    m_capacitorRight = rightMix - rightFiltered * highPassStrength;
    
    // Apply master gain from configuration
    leftFiltered *= m_volume;
    rightFiltered *= m_volume;
    
    // Clamp
    if (leftFiltered > 1.0f) leftFiltered = 1.0f;
    if (leftFiltered < -1.0f) leftFiltered = -1.0f;
    if (rightFiltered > 1.0f) rightFiltered = 1.0f;
    if (rightFiltered < -1.0f) rightFiltered = -1.0f;
    
    float sampleBuffer[2] = { leftFiltered, rightFiltered };
    if (m_audioDevice) {
        m_audioDevice->writeSamples(sampleBuffer, 2 * sizeof(float));
    }
}

void APU::updateNR52() {
    // Update channel enable bits in NR52
    // Bit 0-3: Channel 1-4 on flags (read-only)
    // Bit 7: Master enable (read/write)
}

u8 APU::readRegister(u16 address) const {
    if (!m_enabled && address != 0xFF26) {
        // When APU is disabled, all registers except NR52 read as 0xFF
        return 0xFF;
    }
    
    switch (address) {
        // Square 1
        case 0xFF10: // NR10 - Sweep
            return 0x80 | (m_square1.sweepPeriod << 4) | 
                   (m_square1.sweepNegate ? 0x08 : 0x00) | m_square1.sweepShift;
        case 0xFF11: // NR11 - Length/Duty
            return (m_square1.dutyCycle << 6) | 0x3F;
        case 0xFF12: // NR12 - Volume/Envelope
            return (m_square1.volume << 4) | 
                   (m_square1.envelopeAddMode ? 0x08 : 0x00) | m_square1.envelopePeriod;
        case 0xFF13: // NR13 - Frequency low
            return 0xFF; // Write-only
        case 0xFF14: // NR14 - Frequency high/Control
            return (m_square1.lengthEnable ? 0x40 : 0x00) | 0xBF;
        
        // Square 2
        case 0xFF16: // NR21 - Length/Duty
            return (m_square2.dutyCycle << 6) | 0x3F;
        case 0xFF17: // NR22 - Volume/Envelope
            return (m_square2.volume << 4) | 
                   (m_square2.envelopeAddMode ? 0x08 : 0x00) | m_square2.envelopePeriod;
        case 0xFF18: // NR23 - Frequency low
            return 0xFF; // Write-only
        case 0xFF19: // NR24 - Frequency high/Control
            return (m_square2.lengthEnable ? 0x40 : 0x00) | 0xBF;
        
        // Wave
        case 0xFF1A: // NR30 - DAC enable
            return (m_wave.dacEnabled ? 0x80 : 0x00) | 0x7F;
        case 0xFF1B: // NR31 - Length
            return 0xFF; // Write-only
        case 0xFF1C: // NR32 - Output level
            return (m_wave.outputLevel << 5) | 0x9F;
        case 0xFF1D: // NR33 - Frequency low
            return 0xFF; // Write-only
        case 0xFF1E: // NR34 - Frequency high/Control
            return (m_wave.lengthEnable ? 0x40 : 0x00) | 0xBF;
        
        // Noise
        case 0xFF20: // NR41 - Length
            return 0xFF; // Write-only
        case 0xFF21: // NR42 - Volume/Envelope
            return (m_noise.volume << 4) | 
                   (m_noise.envelopeAddMode ? 0x08 : 0x00) | m_noise.envelopePeriod;
        case 0xFF22: // NR43 - Frequency/Randomness
            return (m_noise.clockShift << 4) | 
                   (m_noise.widthMode ? 0x08 : 0x00) | m_noise.divisorCode;
        case 0xFF23: // NR44 - Control
            return (m_noise.lengthEnable ? 0x40 : 0x00) | 0xBF;
        
        // Master control
        case 0xFF24: // NR50 - Master volume
            return (m_leftVinEnable ? 0x80 : 0x00) | (m_leftVolume << 4) |
                   (m_rightVinEnable ? 0x08 : 0x00) | m_rightVolume;
        case 0xFF25: // NR51 - Sound panning
            return (m_leftEnable << 4) | m_rightEnable;
        case 0xFF26: // NR52 - Sound on/off
            return (m_enabled ? 0x80 : 0x00) | 0x70 |
                   (m_noise.enabled ? 0x08 : 0x00) |
                   (m_wave.enabled ? 0x04 : 0x00) |
                   (m_square2.enabled ? 0x02 : 0x00) |
                   (m_square1.enabled ? 0x01 : 0x00);
        
        // Wave RAM
        case 0xFF30: case 0xFF31: case 0xFF32: case 0xFF33:
        case 0xFF34: case 0xFF35: case 0xFF36: case 0xFF37:
        case 0xFF38: case 0xFF39: case 0xFF3A: case 0xFF3B:
        case 0xFF3C: case 0xFF3D: case 0xFF3E: case 0xFF3F:
            return m_wave.waveRAM[address - 0xFF30];
        
        default:
            return 0xFF;
    }
}

void APU::writeRegister(u16 address, u8 value) {
    if (!m_enabled && address != 0xFF26) {
        // When APU is disabled, ignore all writes except to NR52
        return;
    }
    
    switch (address) {
        // Square 1
        case 0xFF10: // NR10 - Sweep
            m_square1.sweepPeriod = (value >> 4) & 0x07;
            m_square1.sweepNegate = (value & 0x08) != 0;
            m_square1.sweepShift = value & 0x07;
            break;
        case 0xFF11: // NR11 - Length/Duty
            m_square1.dutyCycle = (value >> 6) & 0x03;
            m_square1.lengthCounter = 64 - (value & 0x3F);
            break;
        case 0xFF12: // NR12 - Volume/Envelope
            m_square1.volume = (value >> 4) & 0x0F;
            m_square1.envelopeAddMode = (value & 0x08) != 0;
            m_square1.envelopePeriod = value & 0x07;
            // DAC is enabled if top 5 bits are not all 0
            m_square1.dacEnabled = (value & 0xF8) != 0;
            if (!m_square1.dacEnabled) {
                m_square1.enabled = false;
            }
            break;
        case 0xFF13: // NR13 - Frequency low
            m_square1.frequency = (m_square1.frequency & 0x0700) | value;
            break;
        case 0xFF14: // NR14 - Frequency high/Control
            m_square1.frequency = (m_square1.frequency & 0x00FF) | ((value & 0x07) << 8);
            m_square1.lengthEnable = (value & 0x40) != 0;
            if (value & 0x80) {
                m_square1.trigger(true);
                updateNR52();
            }
            break;
        
        // Square 2
        case 0xFF16: // NR21 - Length/Duty
            m_square2.dutyCycle = (value >> 6) & 0x03;
            m_square2.lengthCounter = 64 - (value & 0x3F);
            break;
        case 0xFF17: // NR22 - Volume/Envelope
            m_square2.volume = (value >> 4) & 0x0F;
            m_square2.envelopeAddMode = (value & 0x08) != 0;
            m_square2.envelopePeriod = value & 0x07;
            m_square2.dacEnabled = (value & 0xF8) != 0;
            if (!m_square2.dacEnabled) {
                m_square2.enabled = false;
            }
            break;
        case 0xFF18: // NR23 - Frequency low
            m_square2.frequency = (m_square2.frequency & 0x0700) | value;
            break;
        case 0xFF19: // NR24 - Frequency high/Control
            m_square2.frequency = (m_square2.frequency & 0x00FF) | ((value & 0x07) << 8);
            m_square2.lengthEnable = (value & 0x40) != 0;
            if (value & 0x80) {
                m_square2.trigger(false);
                updateNR52();
            }
            break;
        
        // Wave
        case 0xFF1A: // NR30 - DAC enable
            m_wave.dacEnabled = (value & 0x80) != 0;
            if (!m_wave.dacEnabled) {
                m_wave.enabled = false;
            }
            break;
        case 0xFF1B: // NR31 - Length
            m_wave.lengthCounter = 256 - value;
            break;
        case 0xFF1C: // NR32 - Output level
            m_wave.outputLevel = (value >> 5) & 0x03;
            break;
        case 0xFF1D: // NR33 - Frequency low
            m_wave.frequency = (m_wave.frequency & 0x0700) | value;
            break;
        case 0xFF1E: // NR34 - Frequency high/Control
            m_wave.frequency = (m_wave.frequency & 0x00FF) | ((value & 0x07) << 8);
            m_wave.lengthEnable = (value & 0x40) != 0;
            if (value & 0x80) {
                m_wave.trigger();
                updateNR52();
            }
            break;
        
        // Noise
        case 0xFF20: // NR41 - Length
            m_noise.lengthCounter = 64 - (value & 0x3F);
            break;
        case 0xFF21: // NR42 - Volume/Envelope
            m_noise.volume = (value >> 4) & 0x0F;
            m_noise.envelopeAddMode = (value & 0x08) != 0;
            m_noise.envelopePeriod = value & 0x07;
            m_noise.dacEnabled = (value & 0xF8) != 0;
            if (!m_noise.dacEnabled) {
                m_noise.enabled = false;
            }
            break;
        case 0xFF22: // NR43 - Frequency/Randomness
            m_noise.clockShift = (value >> 4) & 0x0F;
            m_noise.widthMode = (value & 0x08) != 0;
            m_noise.divisorCode = value & 0x07;
            break;
        case 0xFF23: // NR44 - Control
            m_noise.lengthEnable = (value & 0x40) != 0;
            if (value & 0x80) {
                m_noise.trigger();
                updateNR52();
            }
            break;
        
        // Master control
        case 0xFF24: // NR50 - Master volume
            m_leftVinEnable = (value & 0x80) != 0;
            m_leftVolume = (value >> 4) & 0x07;
            m_rightVinEnable = (value & 0x08) != 0;
            m_rightVolume = value & 0x07;
            break;
        case 0xFF25: // NR51 - Sound panning
            m_leftEnable = (value >> 4) & 0x0F;
            m_rightEnable = value & 0x0F;
            break;
        case 0xFF26: // NR52 - Sound on/off
            {
                bool wasEnabled = m_enabled;
                m_enabled = (value & 0x80) != 0;
                
                // If APU is being disabled, clear all registers
                if (wasEnabled && !m_enabled) {
                    // Clear all channel registers
                    for (u16 addr = 0xFF10; addr <= 0xFF25; addr++) {
                        writeRegister(addr, 0);
                    }
                    
                    m_square1.enabled = false;
                    m_square2.enabled = false;
                    m_wave.enabled = false;
                    m_noise.enabled = false;
                    
                    m_frameSequencerStep = 0;
                }
                // If APU is being enabled, reset frame sequencer
                else if (!wasEnabled && m_enabled) {
                    m_frameSequencerStep = 0;
                }
            }
            break;
        
        // Wave RAM
        case 0xFF30: case 0xFF31: case 0xFF32: case 0xFF33:
        case 0xFF34: case 0xFF35: case 0xFF36: case 0xFF37:
        case 0xFF38: case 0xFF39: case 0xFF3A: case 0xFF3B:
        case 0xFF3C: case 0xFF3D: case 0xFF3E: case 0xFF3F:
            m_wave.waveRAM[address - 0xFF30] = value;
            break;
        
        default:
            break;
    }
}

template <typename Visit>
void APU::visitState(Visit visit) {
    visit(m_square1);
    visit(m_square2);
    visit(m_wave);
    visit(m_noise);

    // Master control
    visit(m_leftVolume);
    visit(m_rightVolume);
    visit(m_leftVinEnable);
    visit(m_rightVinEnable);
    visit(m_leftEnable);
    visit(m_rightEnable);
    visit(m_enabled);

    // Frame sequencer
    visit(m_frameSequencerTimer);
    visit(m_frameSequencerStep);

    // Sample generation
    visit(m_sampleTimer);
    visit(m_cycleAccumulator);
    visit(m_capacitorLeft);
    visit(m_capacitorRight);
}

void APU::saveState(Buffer* buf) {
    visitState(StateWriter{buf});
}

void APU::loadState(Buffer* buf) {
    visitState(StateReader{buf});
}

// SquareChannel implementation
void APU::SquareChannel::reset() {
    sweepPeriod = 0;
    sweepNegate = false;
    sweepShift = 0;
    sweepTimer = 0;
    sweepShadow = 0;
    sweepEnabled = false;
    
    dutyCycle = 0;
    lengthCounter = 0;
    
    volume = 0;
    envelopeAddMode = false;
    envelopePeriod = 0;
    envelopeTimer = 0;
    currentVolume = 0;
    
    frequency = 0;
    lengthEnable = false;
    dacEnabled = false;
    
    enabled = false;
    frequencyTimer = 0;
    dutyPosition = 0;
}

void APU::SquareChannel::trigger(bool hasSweep) {
    enabled = dacEnabled;
    if (!enabled) return;
    
    // Reset length counter if it's 0
    if (lengthCounter == 0) {
        lengthCounter = 64;
    }
    
    // Reset frequency timer
    frequencyTimer = (2048 - frequency) * 4;
    
    // Reset envelope
    envelopeTimer = envelopePeriod;
    currentVolume = volume;
    
    // Reset sweep (channel 1 only)
    if (hasSweep) {
        sweepShadow = frequency;
        sweepTimer = sweepPeriod ? sweepPeriod : 8;
        sweepEnabled = (sweepPeriod != 0 || sweepShift != 0);
        
        // Sweep overflow check
        if (sweepShift != 0) {
            u16 newFreq = sweepShadow >> sweepShift;
            if (sweepNegate) {
                newFreq = sweepShadow - newFreq;
            } else {
                newFreq = sweepShadow + newFreq;
            }
            if (newFreq > 2047) {
                enabled = false;
            }
        }
    }
}

void APU::SquareChannel::clockLength() {
    if (lengthEnable && lengthCounter > 0) {
        lengthCounter--;
        if (lengthCounter == 0) {
            enabled = false;
        }
    }
}

void APU::SquareChannel::clockEnvelope() {
    if (envelopePeriod == 0) return;
    
    if (envelopeTimer > 0) {
        envelopeTimer--;
    }
    
    if (envelopeTimer == 0) {
        envelopeTimer = envelopePeriod;
        
        if (envelopeAddMode && currentVolume < 15) {
            currentVolume++;
        } else if (!envelopeAddMode && currentVolume > 0) {
            currentVolume--;
        }
    }
}

void APU::SquareChannel::clockSweep() {
    if (sweepTimer > 0) {
        sweepTimer--;
    }
    
    if (sweepTimer == 0) {
        sweepTimer = sweepPeriod ? sweepPeriod : 8;
        
        if (sweepEnabled && sweepPeriod != 0) {
            u16 newFreq = sweepShadow >> sweepShift;
            
            if (sweepNegate) {
                newFreq = sweepShadow - newFreq;
            } else {
                newFreq = sweepShadow + newFreq;
            }
            
            // Overflow check
            if (newFreq > 2047) {
                enabled = false;
            } else if (sweepShift != 0) {
                sweepShadow = newFreq;
                frequency = newFreq;
                
                // Second overflow check
                u16 newFreq2 = sweepShadow >> sweepShift;
                if (!sweepNegate) {
                    newFreq2 = sweepShadow + newFreq2;
                }
                if (newFreq2 > 2047) {
                    enabled = false;
                }
            }
        }
    }
}

u32 APU::SquareChannel::getFrequencyTimerPeriod() const {
    // Frequency timer period = (2048 - frequency) * 4
    return (2048 - frequency) * 4;
}

s16 APU::SquareChannel::getOutput() const {
    if (!enabled || !dacEnabled) {
        return 0;
    }
    
    // Play inaudible frequencies as constant volume
    if (frequency >= 0x7FA) {
        return currentVolume;
    }

    // Get duty pattern output
    u8 dutyBit = APU::DUTY_PATTERNS[dutyCycle][dutyPosition];
    
    // Return volume if duty is 1, otherwise 0
    return dutyBit ? currentVolume : 0;
}

// WaveChannel implementation
void APU::WaveChannel::reset() {
    dacEnabled = false;
    lengthCounter = 0;
    outputLevel = 0;
    frequency = 0;
    lengthEnable = false;
    
    enabled = false;
    frequencyTimer = 0;
    wavePosition = 0;
    
    std::fill(waveRAM.begin(), waveRAM.end(), 0);
}

void APU::WaveChannel::trigger() {
    enabled = dacEnabled;
    if (!enabled) return;
    
    // Reset length counter if it's 0
    if (lengthCounter == 0) {
        lengthCounter = 256;
    }
    
    // Reset frequency timer
    frequencyTimer = (2048 - frequency) * 2;
    
    // Reset wave position
    wavePosition = 0;
}

void APU::WaveChannel::clockLength() {
    if (lengthEnable && lengthCounter > 0) {
        lengthCounter--;
        if (lengthCounter == 0) {
            enabled = false;
        }
    }
}

s16 APU::WaveChannel::getOutput() const {
    if (!enabled || !dacEnabled) {
        return 0;
    }
    
    // Get sample from wave RAM
    u8 byte = waveRAM[wavePosition / 2];
    u8 sample = (wavePosition & 1) ? (byte & 0x0F) : (byte >> 4);
    
    // Apply output level
    if (outputLevel == 0) {
        return 0; // Mute
    } else {
        // outputLevel: 1=100%, 2=50%, 3=25%
        return sample >> (outputLevel - 1);
    }
}

// NoiseChannel implementation
void APU::NoiseChannel::reset() {
    lengthCounter = 0;
    volume = 0;
    envelopeAddMode = false;
    envelopePeriod = 0;
    envelopeTimer = 0;
    currentVolume = 0;
    clockShift = 0;
    widthMode = false;
    divisorCode = 0;
    lengthEnable = false;
    dacEnabled = false;
    enabled = false;
    frequencyTimer = 0;
    lfsr = 0;  // LFSR starts at 0
}

void APU::NoiseChannel::trigger() {
    enabled = dacEnabled;
    if (!enabled) return;
    
    // Reset length counter if it's 0
    if (lengthCounter == 0) {
        lengthCounter = 64;
    }
    
    // Reset envelope
    envelopeTimer = envelopePeriod;
    currentVolume = volume;
    
    // Reset frequency timer
    static const u32 divisors[8] = {8, 16, 32, 48, 64, 80, 96, 112};
    u32 divisor = divisors[divisorCode];
    frequencyTimer = divisor << clockShift;
    
    // Reset LFSR to 0 (as per hardware documentation)
    lfsr = 0;
}

void APU::NoiseChannel::clockLength() {
    if (lengthEnable && lengthCounter > 0) {
        lengthCounter--;
        if (lengthCounter == 0) {
            enabled = false;
        }
    }
}

void APU::NoiseChannel::clockEnvelope() {
    if (envelopePeriod == 0) return;
    
    if (envelopeTimer > 0) {
        envelopeTimer--;
    }
    
    if (envelopeTimer == 0) {
        envelopeTimer = envelopePeriod;
        
        if (envelopeAddMode && currentVolume < 15) {
            currentVolume++;
        } else if (!envelopeAddMode && currentVolume > 0) {
            currentVolume--;
        }
    }
}

u32 APU::NoiseChannel::getFrequencyPeriod() const {
    // Noise frequency is determined by divisor and clock shift
    // Period = divisor * 2^(shift+1)
    
    static const u32 divisors[8] = {8, 16, 32, 48, 64, 80, 96, 112};
    u32 divisor = divisors[divisorCode];
    u32 period = divisor << clockShift;
    
    return period;
}

s16 APU::NoiseChannel::getOutput() const {
    if (!enabled || !dacEnabled) {
        return 0;
    }
    
    // Return volume if LFSR bit 0 is 0, otherwise 0
    return (lfsr & 1) ? 0 : currentVolume;
}

} // namespace gb

