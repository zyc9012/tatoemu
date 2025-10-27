#include "apu.h"
#include "cpu.h"
#include <cstring>
#include <cmath>
#include <iostream>

APU::APU()
    : m_nr50(0x77)
    , m_nr51(0xF3)
    , m_nr52(0xF1)
    , m_frameSequencerCounter(0)
    , m_frameSequencer(0)
    , m_audioStream(nullptr)
    , m_cpu(nullptr) {
    reset();
}

APU::~APU() {
    closeAudio();
}

void APU::setCPU(CPU* cpu) {
    m_cpu = cpu;
}

void APU::reset() {
    // Reset Channel 1
    m_channel1 = {};
    m_channel1.enabled = false;
    m_channel1.lengthCounter = 64;
    
    // Reset Channel 2
    m_channel2 = {};
    m_channel2.enabled = false;
    m_channel2.lengthCounter = 64;
    
    // Reset Channel 3
    m_channel3 = {};
    m_channel3.enabled = false;
    m_channel3.lengthCounter = 256;
    std::fill(m_channel3.waveRAM.begin(), m_channel3.waveRAM.end(), 0);
    
    // Reset Channel 4
    m_channel4 = {};
    m_channel4.enabled = false;
    m_channel4.lengthCounter = 64;
    m_channel4.lfsr = 0x7FFF;
    
    // Reset master control
    m_nr50 = 0x77;
    m_nr51 = 0xF3;
    m_nr52 = 0xF1;
    
    m_frameSequencerCounter = 0;
    m_frameSequencer = 0;
}

bool APU::initializeAudio() {
    SDL_AudioSpec spec;
    spec.freq = SAMPLE_RATE;
    spec.format = SDL_AUDIO_F32;
    spec.channels = 2;  // Stereo
    
    m_audioStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    
    if (!m_audioStream) {
        std::cerr << "Failed to open audio stream: " << SDL_GetError() << std::endl;
        return false;
    }
    
    SDL_ResumeAudioStreamDevice(m_audioStream);
    
    std::cout << "Audio initialized: " << SAMPLE_RATE << "Hz, Stereo" << std::endl;
    return true;
}

void APU::closeAudio() {
    if (m_audioStream) {
        SDL_DestroyAudioStream(m_audioStream);
        m_audioStream = nullptr;
    }
}

void APU::step(u32 cycles) {
    if (!(m_nr52 & 0x80)) {
        // APU is disabled
        return;
    }
    
    // Update all channel frequency counters at CPU clock speed
    for (u32 i = 0; i < cycles; i++) {
        // Channel 1 - Square with sweep
        if (m_channel1.enabled) {
            m_channel1.frequencyCounter++;
            if (m_channel1.frequencyCounter >= (2048 - m_channel1.frequency) * 4) {
                m_channel1.frequencyCounter = 0;
                m_channel1.dutyPosition = (m_channel1.dutyPosition + 1) % 8;
            }
        }
        
        // Channel 2 - Square
        if (m_channel2.enabled) {
            m_channel2.frequencyCounter++;
            if (m_channel2.frequencyCounter >= (2048 - m_channel2.frequency) * 4) {
                m_channel2.frequencyCounter = 0;
                m_channel2.dutyPosition = (m_channel2.dutyPosition + 1) % 8;
            }
        }
        
        // Channel 3 - Wave
        if (m_channel3.enabled && (m_channel3.onOff & 0x80)) {
            m_channel3.frequencyCounter++;
            if (m_channel3.frequencyCounter >= (2048 - m_channel3.frequency) * 2) {
                m_channel3.frequencyCounter = 0;
                m_channel3.wavePosition = (m_channel3.wavePosition + 1) % 32;
            }
        }
        
        // Channel 4 - Noise
        if (m_channel4.enabled) {
            u8 divisor = m_channel4.polynomial & 0x07;
            u8 shift = (m_channel4.polynomial >> 4) & 0x0F;
            u16 divisorCode = divisor == 0 ? 8 : divisor * 16;
            u16 period = divisorCode << shift;
            
            m_channel4.clockCounter++;
            if (m_channel4.clockCounter >= period) {
                m_channel4.clockCounter = 0;
                
                // LFSR step
                u16 bit = (m_channel4.lfsr & 0x01) ^ ((m_channel4.lfsr >> 1) & 0x01);
                m_channel4.lfsr >>= 1;
                m_channel4.lfsr |= (bit << 14);
                
                // 7-bit mode
                if (m_channel4.polynomial & 0x08) {
                    m_channel4.lfsr &= ~0x40;
                    m_channel4.lfsr |= (bit << 6);
                }
            }
        }
    }
    
    // Frame sequencer runs at 512 Hz (every 8192 cycles)
    m_frameSequencerCounter += cycles;
    while (m_frameSequencerCounter >= 8192) {
        m_frameSequencerCounter -= 8192;
        tickFrameSequencer();
    }
    
    // Generate audio samples at the appropriate rate
    // Game Boy CPU runs at ~4.194 MHz, we want 44.1 kHz samples
    // So we generate a sample approximately every 95 cycles
    static u32 sampleCounter = 0;
    sampleCounter += cycles;
    
    const u32 cyclesPerSample = CLOCK_SPEED / SAMPLE_RATE;  // ~95 cycles
    
    while (sampleCounter >= cyclesPerSample) {
        sampleCounter -= cyclesPerSample;
        
        // Generate stereo samples
        float samples[2];
        samples[0] = 0.0f;  // Left
        samples[1] = 0.0f;  // Right
        
        // Mix channels based on NR51
        if (m_nr52 & 0x80) {
            float ch1 = getChannel1Sample();
            float ch2 = getChannel2Sample();
            float ch3 = getChannel3Sample();
            float ch4 = getChannel4Sample();
            
            // Left output (NR51 bits 4-7)
            if (m_nr51 & 0x10) samples[0] += ch1;
            if (m_nr51 & 0x20) samples[0] += ch2;
            if (m_nr51 & 0x40) samples[0] += ch3;
            if (m_nr51 & 0x80) samples[0] += ch4;
            
            // Right output (NR51 bits 0-3)
            if (m_nr51 & 0x01) samples[1] += ch1;
            if (m_nr51 & 0x02) samples[1] += ch2;
            if (m_nr51 & 0x04) samples[1] += ch3;
            if (m_nr51 & 0x08) samples[1] += ch4;
            
            // Apply master volume (NR50)
            float leftVolume = ((m_nr50 >> 4) & 0x07) / 7.0f;
            float rightVolume = (m_nr50 & 0x07) / 7.0f;
            
            samples[0] *= leftVolume * 0.25f;  // Scale down to prevent clipping
            samples[1] *= rightVolume * 0.25f;
        }
        
        // Push to audio stream
        if (m_audioStream) {
            SDL_PutAudioStreamData(m_audioStream, samples, sizeof(samples));
        }
    }
}

void APU::tickFrameSequencer() {
    // Frame sequencer ticks at 512 Hz
    // Step 0, 2, 4, 6: Clock length
    // Step 2, 6: Clock sweep
    // Step 7: Clock volume envelope
    
    if (m_frameSequencer % 2 == 0) {
        tickLengthCounters();
    }
    
    if (m_frameSequencer == 2 || m_frameSequencer == 6) {
        tickSweep();
    }
    
    if (m_frameSequencer == 7) {
        tickVolumeEnvelopes();
    }
    
    m_frameSequencer = (m_frameSequencer + 1) % 8;
}

void APU::tickLengthCounters() {
    // Channel 1
    if ((m_channel1.freqHigh & 0x40) && m_channel1.lengthCounter > 0) {
        m_channel1.lengthCounter--;
        if (m_channel1.lengthCounter == 0) {
            m_channel1.enabled = false;
            m_nr52 &= ~0x01;
        }
    }
    
    // Channel 2
    if ((m_channel2.freqHigh & 0x40) && m_channel2.lengthCounter > 0) {
        m_channel2.lengthCounter--;
        if (m_channel2.lengthCounter == 0) {
            m_channel2.enabled = false;
            m_nr52 &= ~0x02;
        }
    }
    
    // Channel 3
    if ((m_channel3.freqHigh & 0x40) && m_channel3.lengthCounter > 0) {
        m_channel3.lengthCounter--;
        if (m_channel3.lengthCounter == 0) {
            m_channel3.enabled = false;
            m_nr52 &= ~0x04;
        }
    }
    
    // Channel 4
    if ((m_channel4.control & 0x40) && m_channel4.lengthCounter > 0) {
        m_channel4.lengthCounter--;
        if (m_channel4.lengthCounter == 0) {
            m_channel4.enabled = false;
            m_nr52 &= ~0x08;
        }
    }
}

void APU::tickVolumeEnvelopes() {
    // Channel 1
    if (m_channel1.envelopePeriod > 0) {
        if (m_channel1.envelopeCounter > 0) {
            m_channel1.envelopeCounter--;
        }
        if (m_channel1.envelopeCounter == 0) {
            m_channel1.envelopeCounter = m_channel1.envelopePeriod;
            if (m_channel1.envelopeIncrease && m_channel1.volume < 15) {
                m_channel1.volume++;
            } else if (!m_channel1.envelopeIncrease && m_channel1.volume > 0) {
                m_channel1.volume--;
            }
        }
    }
    
    // Channel 2
    if (m_channel2.envelopePeriod > 0) {
        if (m_channel2.envelopeCounter > 0) {
            m_channel2.envelopeCounter--;
        }
        if (m_channel2.envelopeCounter == 0) {
            m_channel2.envelopeCounter = m_channel2.envelopePeriod;
            if (m_channel2.envelopeIncrease && m_channel2.volume < 15) {
                m_channel2.volume++;
            } else if (!m_channel2.envelopeIncrease && m_channel2.volume > 0) {
                m_channel2.volume--;
            }
        }
    }
    
    // Channel 4
    if (m_channel4.envelopePeriod > 0) {
        if (m_channel4.envelopeCounter > 0) {
            m_channel4.envelopeCounter--;
        }
        if (m_channel4.envelopeCounter == 0) {
            m_channel4.envelopeCounter = m_channel4.envelopePeriod;
            if (m_channel4.envelopeIncrease && m_channel4.volume < 15) {
                m_channel4.volume++;
            } else if (!m_channel4.envelopeIncrease && m_channel4.volume > 0) {
                m_channel4.volume--;
            }
        }
    }
}

void APU::tickSweep() {
    if (m_channel1.sweepPeriod > 0) {
        if (m_channel1.sweepCounter > 0) {
            m_channel1.sweepCounter--;
        }
        if (m_channel1.sweepCounter == 0) {
            m_channel1.sweepCounter = m_channel1.sweepPeriod;
            
            if (m_channel1.sweepShift > 0) {
                u16 delta = m_channel1.frequency >> m_channel1.sweepShift;
                u16 newFreq;
                
                if (m_channel1.sweepIncrease) {
                    newFreq = m_channel1.frequency + delta;
                    if (newFreq > 2047) {
                        m_channel1.enabled = false;
                        m_nr52 &= ~0x01;
                    } else {
                        m_channel1.frequency = newFreq;
                    }
                } else {
                    if (delta > m_channel1.frequency) {
                        m_channel1.enabled = false;
                        m_nr52 &= ~0x01;
                    } else {
                        m_channel1.frequency = m_channel1.frequency - delta;
                    }
                }
            }
        }
    }
}

float APU::getChannel1Sample() {
    if (!m_channel1.enabled) return 0.0f;
    
    // Duty cycle patterns
    static const u8 dutyPatterns[4][8] = {
        {0, 0, 0, 0, 0, 0, 0, 1},  // 12.5%
        {1, 0, 0, 0, 0, 0, 0, 1},  // 25%
        {1, 0, 0, 0, 0, 1, 1, 1},  // 50%
        {0, 1, 1, 1, 1, 1, 1, 0},  // 75%
    };
    
    u8 output = dutyPatterns[m_channel1.duty][m_channel1.dutyPosition];
    return output ? (m_channel1.volume / 15.0f) : 0.0f;
}

float APU::getChannel2Sample() {
    if (!m_channel2.enabled) return 0.0f;
    
    // Duty cycle patterns
    static const u8 dutyPatterns[4][8] = {
        {0, 0, 0, 0, 0, 0, 0, 1},  // 12.5%
        {1, 0, 0, 0, 0, 0, 0, 1},  // 25%
        {1, 0, 0, 0, 0, 1, 1, 1},  // 50%
        {0, 1, 1, 1, 1, 1, 1, 0},  // 75%
    };
    
    u8 output = dutyPatterns[m_channel2.duty][m_channel2.dutyPosition];
    return output ? (m_channel2.volume / 15.0f) : 0.0f;
}

float APU::getChannel3Sample() {
    if (!m_channel3.enabled || !(m_channel3.onOff & 0x80)) return 0.0f;
    
    // Get sample from wave RAM (4-bit samples, 2 per byte)
    u8 byteIndex = m_channel3.wavePosition / 2;
    u8 sample = m_channel3.waveRAM[byteIndex];
    
    if (m_channel3.wavePosition % 2 == 0) {
        sample = (sample >> 4) & 0x0F;
    } else {
        sample = sample & 0x0F;
    }
    
    // Apply output level
    u8 shift = (m_channel3.outputLevel >> 5) & 0x03;
    if (shift > 0) {
        sample >>= (shift - 1);
    } else {
        sample = 0;  // Mute
    }
    
    return sample / 15.0f;
}

float APU::getChannel4Sample() {
    if (!m_channel4.enabled) return 0.0f;
    
    u8 output = (~m_channel4.lfsr) & 0x01;
    return output ? (m_channel4.volume / 15.0f) : 0.0f;
}

void APU::triggerChannel1() {
    m_channel1.enabled = true;
    m_nr52 |= 0x01;
    
    if (m_channel1.lengthCounter == 0) {
        m_channel1.lengthCounter = 64;
    }
    
    m_channel1.frequencyCounter = 0;
    m_channel1.volume = (m_channel1.envelope >> 4) & 0x0F;
    m_channel1.envelopePeriod = m_channel1.envelope & 0x07;
    m_channel1.envelopeCounter = m_channel1.envelopePeriod;
    m_channel1.envelopeIncrease = (m_channel1.envelope & 0x08) != 0;
    
    // Sweep
    m_channel1.sweepPeriod = (m_channel1.sweep >> 4) & 0x07;
    m_channel1.sweepCounter = m_channel1.sweepPeriod;
    m_channel1.sweepShift = m_channel1.sweep & 0x07;
    m_channel1.sweepIncrease = !(m_channel1.sweep & 0x08);
}

void APU::triggerChannel2() {
    m_channel2.enabled = true;
    m_nr52 |= 0x02;
    
    if (m_channel2.lengthCounter == 0) {
        m_channel2.lengthCounter = 64;
    }
    
    m_channel2.frequencyCounter = 0;
    m_channel2.volume = (m_channel2.envelope >> 4) & 0x0F;
    m_channel2.envelopePeriod = m_channel2.envelope & 0x07;
    m_channel2.envelopeCounter = m_channel2.envelopePeriod;
    m_channel2.envelopeIncrease = (m_channel2.envelope & 0x08) != 0;
}

void APU::triggerChannel3() {
    m_channel3.enabled = true;
    m_nr52 |= 0x04;
    
    if (m_channel3.lengthCounter == 0) {
        m_channel3.lengthCounter = 256;
    }
    
    m_channel3.frequencyCounter = 0;
    m_channel3.wavePosition = 0;
}

void APU::triggerChannel4() {
    m_channel4.enabled = true;
    m_nr52 |= 0x08;
    
    if (m_channel4.lengthCounter == 0) {
        m_channel4.lengthCounter = 64;
    }
    
    m_channel4.clockCounter = 0;
    m_channel4.lfsr = 0x7FFF;
    m_channel4.volume = (m_channel4.envelope >> 4) & 0x0F;
    m_channel4.envelopePeriod = m_channel4.envelope & 0x07;
    m_channel4.envelopeCounter = m_channel4.envelopePeriod;
    m_channel4.envelopeIncrease = (m_channel4.envelope & 0x08) != 0;
}

u8 APU::readRegister(u16 address) const {
    switch (address) {
        // Channel 1
        case 0xFF10: return m_channel1.sweep | 0x80;
        case 0xFF11: return m_channel1.lengthDuty | 0x3F;
        case 0xFF12: return m_channel1.envelope;
        case 0xFF13: return 0xFF;  // Write-only
        case 0xFF14: return m_channel1.freqHigh | 0xBF;
        
        // Channel 2
        case 0xFF16: return m_channel2.lengthDuty | 0x3F;
        case 0xFF17: return m_channel2.envelope;
        case 0xFF18: return 0xFF;  // Write-only
        case 0xFF19: return m_channel2.freqHigh | 0xBF;
        
        // Channel 3
        case 0xFF1A: return m_channel3.onOff | 0x7F;
        case 0xFF1B: return 0xFF;  // Write-only
        case 0xFF1C: return m_channel3.outputLevel | 0x9F;
        case 0xFF1D: return 0xFF;  // Write-only
        case 0xFF1E: return m_channel3.freqHigh | 0xBF;
        
        // Channel 4
        case 0xFF20: return 0xFF;  // Write-only
        case 0xFF21: return m_channel4.envelope;
        case 0xFF22: return m_channel4.polynomial;
        case 0xFF23: return m_channel4.control | 0xBF;
        
        // Control
        case 0xFF24: return m_nr50;
        case 0xFF25: return m_nr51;
        case 0xFF26: return m_nr52 | 0x70;
        
        // Wave RAM
        case 0xFF30: case 0xFF31: case 0xFF32: case 0xFF33:
        case 0xFF34: case 0xFF35: case 0xFF36: case 0xFF37:
        case 0xFF38: case 0xFF39: case 0xFF3A: case 0xFF3B:
        case 0xFF3C: case 0xFF3D: case 0xFF3E: case 0xFF3F:
            return m_channel3.waveRAM[address - 0xFF30];
        
        default:
            return 0xFF;
    }
}

void APU::writeRegister(u16 address, u8 value) {
    // If APU is off, only NR52 can be written
    if (!(m_nr52 & 0x80) && address != 0xFF26) {
        return;
    }
    
    switch (address) {
        // Channel 1
        case 0xFF10:
            m_channel1.sweep = value;
            break;
        case 0xFF11:
            m_channel1.lengthDuty = value;
            m_channel1.duty = (value >> 6) & 0x03;
            m_channel1.lengthCounter = 64 - (value & 0x3F);
            break;
        case 0xFF12:
            m_channel1.envelope = value;
            // Disable channel if DAC is off (top 5 bits = 0)
            if ((value & 0xF8) == 0) {
                m_channel1.enabled = false;
                m_nr52 &= ~0x01;
            }
            break;
        case 0xFF13:
            m_channel1.freqLow = value;
            m_channel1.frequency = (m_channel1.frequency & 0x700) | value;
            break;
        case 0xFF14:
            m_channel1.freqHigh = value;
            m_channel1.frequency = (m_channel1.frequency & 0xFF) | ((value & 0x07) << 8);
            if (value & 0x80) {
                triggerChannel1();
            }
            break;
        
        // Channel 2
        case 0xFF16:
            m_channel2.lengthDuty = value;
            m_channel2.duty = (value >> 6) & 0x03;
            m_channel2.lengthCounter = 64 - (value & 0x3F);
            break;
        case 0xFF17:
            m_channel2.envelope = value;
            // Disable channel if DAC is off
            if ((value & 0xF8) == 0) {
                m_channel2.enabled = false;
                m_nr52 &= ~0x02;
            }
            break;
        case 0xFF18:
            m_channel2.freqLow = value;
            m_channel2.frequency = (m_channel2.frequency & 0x700) | value;
            break;
        case 0xFF19:
            m_channel2.freqHigh = value;
            m_channel2.frequency = (m_channel2.frequency & 0xFF) | ((value & 0x07) << 8);
            if (value & 0x80) {
                triggerChannel2();
            }
            break;
        
        // Channel 3
        case 0xFF1A:
            m_channel3.onOff = value;
            if (!(value & 0x80)) {
                m_channel3.enabled = false;
                m_nr52 &= ~0x04;
            }
            break;
        case 0xFF1B:
            m_channel3.length = value;
            m_channel3.lengthCounter = 256 - value;
            break;
        case 0xFF1C:
            m_channel3.outputLevel = value;
            break;
        case 0xFF1D:
            m_channel3.freqLow = value;
            m_channel3.frequency = (m_channel3.frequency & 0x700) | value;
            break;
        case 0xFF1E:
            m_channel3.freqHigh = value;
            m_channel3.frequency = (m_channel3.frequency & 0xFF) | ((value & 0x07) << 8);
            if (value & 0x80) {
                triggerChannel3();
            }
            break;
        
        // Channel 4
        case 0xFF20:
            m_channel4.length = value;
            m_channel4.lengthCounter = 64 - (value & 0x3F);
            break;
        case 0xFF21:
            m_channel4.envelope = value;
            // Disable channel if DAC is off
            if ((value & 0xF8) == 0) {
                m_channel4.enabled = false;
                m_nr52 &= ~0x08;
            }
            break;
        case 0xFF22:
            m_channel4.polynomial = value;
            break;
        case 0xFF23:
            m_channel4.control = value;
            if (value & 0x80) {
                triggerChannel4();
            }
            break;
        
        // Control
        case 0xFF24:
            m_nr50 = value;
            break;
        case 0xFF25:
            m_nr51 = value;
            break;
        case 0xFF26:
            m_nr52 = (m_nr52 & 0x0F) | (value & 0x80);
            if (!(value & 0x80)) {
                // APU turned off, reset all registers
                reset();
            }
            break;
        
        // Wave RAM
        case 0xFF30: case 0xFF31: case 0xFF32: case 0xFF33:
        case 0xFF34: case 0xFF35: case 0xFF36: case 0xFF37:
        case 0xFF38: case 0xFF39: case 0xFF3A: case 0xFF3B:
        case 0xFF3C: case 0xFF3D: case 0xFF3E: case 0xFF3F:
            m_channel3.waveRAM[address - 0xFF30] = value;
            break;
    }
}

