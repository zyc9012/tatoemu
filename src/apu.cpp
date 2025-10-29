#include "apu.h"
#include "cpu.h"
#include "mmu.h"
#include <cstring>
#include <cmath>
#include <iostream>

APU::APU()
    : m_nr50(0x77)
    , m_nr51(0xF3)
    , m_nr52(0xF1)
    , m_frameSequencerCounter(0)
    , m_frameSequencer(0)
    , m_sampleCounter(0)
    , m_audioStream(nullptr)
    , m_cpu(nullptr)
    , m_mmu(nullptr) {
    reset();
}

APU::~APU() {
    closeAudio();
}

void APU::saveState(std::ofstream& file) const {
    file.write(reinterpret_cast<const char*>(&m_channel1), sizeof(m_channel1));
    file.write(reinterpret_cast<const char*>(&m_channel2), sizeof(m_channel2));
    file.write(reinterpret_cast<const char*>(&m_channel3), sizeof(m_channel3));
    file.write(reinterpret_cast<const char*>(&m_channel4), sizeof(m_channel4));
    file.write(reinterpret_cast<const char*>(&m_nr50), sizeof(m_nr50));
    file.write(reinterpret_cast<const char*>(&m_nr51), sizeof(m_nr51));
    file.write(reinterpret_cast<const char*>(&m_nr52), sizeof(m_nr52));
    file.write(reinterpret_cast<const char*>(&m_frameSequencerCounter), sizeof(m_frameSequencerCounter));
    file.write(reinterpret_cast<const char*>(&m_frameSequencer), sizeof(m_frameSequencer));
    file.write(reinterpret_cast<const char*>(&m_sampleCounter), sizeof(m_sampleCounter));
}

void APU::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(&m_channel1), sizeof(m_channel1));
    file.read(reinterpret_cast<char*>(&m_channel2), sizeof(m_channel2));
    file.read(reinterpret_cast<char*>(&m_channel3), sizeof(m_channel3));
    file.read(reinterpret_cast<char*>(&m_channel4), sizeof(m_channel4));
    file.read(reinterpret_cast<char*>(&m_nr50), sizeof(m_nr50));
    file.read(reinterpret_cast<char*>(&m_nr51), sizeof(m_nr51));
    file.read(reinterpret_cast<char*>(&m_nr52), sizeof(m_nr52));
    file.read(reinterpret_cast<char*>(&m_frameSequencerCounter), sizeof(m_frameSequencerCounter));
    file.read(reinterpret_cast<char*>(&m_frameSequencer), sizeof(m_frameSequencer));
    file.read(reinterpret_cast<char*>(&m_sampleCounter), sizeof(m_sampleCounter));
    
    // Clear audio buffer after loading state to prevent audio glitches
    clearAudioBuffer();
}

void APU::setCPU(CPU* cpu) {
    m_cpu = cpu;
}

void APU::setMMU(MMU* mmu) {
    m_mmu = mmu;
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
        // Pause the audio device before destroying the stream
        SDL_PauseAudioStreamDevice(m_audioStream);
        // Flush any remaining audio data
        SDL_FlushAudioStream(m_audioStream);
        // Now safely destroy the stream
        SDL_DestroyAudioStream(m_audioStream);
        m_audioStream = nullptr;
    }
}

void APU::clearAudioBuffer() {
    if (m_audioStream) {
        SDL_ClearAudioStream(m_audioStream);
    }
    // Reset sample counter to prevent timing issues
    m_sampleCounter = 0;
}

int APU::getQueuedAudioSize() const {
    if (m_audioStream) {
        return SDL_GetAudioStreamQueued(m_audioStream);
    }
    return 0;
}

void APU::step(u32 cycles) {
    if (!(m_nr52 & 0x80)) {
        // APU is disabled
        return;
    }
    
    // Update all channel frequency counters at CPU clock speed
    // Note: Game Boy APU uses a timing factor of 2 for DMG, so periods are doubled
    for (u32 i = 0; i < cycles; i++) {
        // Channel 1 - Square with sweep
        // Period = (2048 - frequency) * 4 * timingFactor(2) = (2048 - frequency) * 8
        if (m_channel1.enabled) {
            m_channel1.frequencyCounter++;
            if (m_channel1.frequencyCounter >= (2048 - m_channel1.frequency) * 8) {
                m_channel1.frequencyCounter = 0;
                m_channel1.dutyPosition = (m_channel1.dutyPosition + 1) % 8;
            }
        }
        
        // Channel 2 - Square
        // Period = (2048 - frequency) * 4 * timingFactor(2) = (2048 - frequency) * 8
        if (m_channel2.enabled) {
            m_channel2.frequencyCounter++;
            if (m_channel2.frequencyCounter >= (2048 - m_channel2.frequency) * 8) {
                m_channel2.frequencyCounter = 0;
                m_channel2.dutyPosition = (m_channel2.dutyPosition + 1) % 8;
            }
        }
        
        // Channel 3 - Wave
        // Period = (2048 - frequency) * 2 * timingFactor(2) = (2048 - frequency) * 4
        if (m_channel3.enabled && (m_channel3.onOff & 0x80)) {
            m_channel3.frequencyCounter++;
            if (m_channel3.frequencyCounter >= (2048 - m_channel3.frequency) * 4) {
                m_channel3.frequencyCounter = 0;
                m_channel3.wavePosition = (m_channel3.wavePosition + 1) % 32;
            }
        }
        
        // Channel 4 - Noise
        // Period = (divisor ? 2*divisor : 1) << shift * 8 * timingFactor(2)
        // Simplifies to: (divisor ? divisor*2 : 1) << shift * 16
        if (m_channel4.enabled) {
            u8 divisor = m_channel4.polynomial & 0x07;
            u8 shift = (m_channel4.polynomial >> 4) & 0x0F;
            // Doubled from previous values to account for timingFactor of 2
            u16 divisorCode = divisor == 0 ? 16 : divisor * 32;
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
    
    // Frame sequencer runs at 512 Hz (every 8192 cycles in normal speed)
    // In double speed mode, this threshold doubles to maintain real-time rate
    u32 speedMultiplier = (m_mmu && m_mmu->isDoubleSpeed()) ? 2 : 1;
    m_frameSequencerCounter += cycles;
    u32 frameSequencerThreshold = 8192 * speedMultiplier;
    while (m_frameSequencerCounter >= frameSequencerThreshold) {
        m_frameSequencerCounter -= frameSequencerThreshold;
        tickFrameSequencer();
    }
    
    // Generate audio samples at the appropriate rate
    // Game Boy CPU runs at ~4.194 MHz (or 8.388 MHz in double speed), we want 44.1 kHz samples
    // In normal speed: ~95 cycles per sample
    // In double speed: ~190 cycles per sample (to maintain same real-time sample rate)
    m_sampleCounter += cycles;
    
    u32 clockSpeed = (m_mmu && m_mmu->isDoubleSpeed()) ? CLOCK_SPEED_DOUBLE : CLOCK_SPEED;
    const u32 cyclesPerSample = clockSpeed / SAMPLE_RATE;
    
    while (m_sampleCounter >= cyclesPerSample) {
        m_sampleCounter -= cyclesPerSample;
        
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
    if (m_channel1.envelopePeriod > 0 && m_channel1.enabled) {
        if (m_channel1.envelopeCounter > 0) {
            m_channel1.envelopeCounter--;
        }
        if (m_channel1.envelopeCounter == 0) {
            m_channel1.envelopeCounter = m_channel1.envelopePeriod;
            
            if (m_channel1.envelopeIncrease) {
                if (m_channel1.volume < 15) {
                    m_channel1.volume++;
                }
            } else {
                if (m_channel1.volume > 0) {
                    m_channel1.volume--;
                }
            }
        }
    }
    
    // Channel 2
    if (m_channel2.envelopePeriod > 0 && m_channel2.enabled) {
        if (m_channel2.envelopeCounter > 0) {
            m_channel2.envelopeCounter--;
        }
        if (m_channel2.envelopeCounter == 0) {
            m_channel2.envelopeCounter = m_channel2.envelopePeriod;
            
            if (m_channel2.envelopeIncrease) {
                if (m_channel2.volume < 15) {
                    m_channel2.volume++;
                }
            } else {
                if (m_channel2.volume > 0) {
                    m_channel2.volume--;
                }
            }
        }
    }
    
    // Channel 4
    if (m_channel4.envelopePeriod > 0 && m_channel4.enabled) {
        if (m_channel4.envelopeCounter > 0) {
            m_channel4.envelopeCounter--;
        }
        if (m_channel4.envelopeCounter == 0) {
            m_channel4.envelopeCounter = m_channel4.envelopePeriod;
            
            if (m_channel4.envelopeIncrease) {
                if (m_channel4.volume < 15) {
                    m_channel4.volume++;
                }
            } else {
                if (m_channel4.volume > 0) {
                    m_channel4.volume--;
                }
            }
        }
    }
}

void APU::tickSweep() {
    // Sweep only ticks if enabled
    if (!m_channel1.enabled) return;
    
    // Check if sweep should tick (period != 8 means sweep is enabled)
    if (m_channel1.sweepPeriod != 8) {
        if (m_channel1.sweepCounter > 0) {
            m_channel1.sweepCounter--;
        }
        if (m_channel1.sweepCounter == 0) {
            m_channel1.sweepCounter = m_channel1.sweepPeriod;
            
            if (m_channel1.sweepShift > 0) {
                u16 delta = m_channel1.frequency >> m_channel1.sweepShift;
                u16 newFreq;
                
                // sweepIncrease is inverted: false = add (increase), true = subtract (decrease)
                if (m_channel1.sweepIncrease) {
                    // Subtract (decrease frequency)
                    if (delta > m_channel1.frequency) {
                        newFreq = 0;
                    } else {
                        newFreq = m_channel1.frequency - delta;
                    }
                    m_channel1.frequency = newFreq;
                } else {
                    // Add (increase frequency)
                    newFreq = m_channel1.frequency + delta;
                    if (newFreq > 2047) {
                        // Overflow - disable channel
                        m_channel1.enabled = false;
                        m_nr52 &= ~0x01;
                        return;
                    }
                    m_channel1.frequency = newFreq;
                    
                    // Perform overflow check again after writing
                    u16 checkFreq = m_channel1.frequency + (m_channel1.frequency >> m_channel1.sweepShift);
                    if (checkFreq > 2047) {
                        m_channel1.enabled = false;
                        m_nr52 &= ~0x01;
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
    
    // Apply output level (NR32 bits 5-6)
    // mGBA uses: 0 = shift by 4 (mute), 1 = no shift, 2 = shift by 1, 3 = shift by 2
    u8 volumeCode = (m_channel3.outputLevel >> 5) & 0x03;
    u8 shift;
    switch (volumeCode) {
        case 0: shift = 4; break;  // Mute
        case 1: shift = 0; break;  // Full volume
        case 2: shift = 1; break;  // Half volume
        case 3: shift = 2; break;  // Quarter volume
        default: shift = 4; break;
    }
    
    sample >>= shift;
    
    return sample / 15.0f;
}

float APU::getChannel4Sample() {
    if (!m_channel4.enabled) return 0.0f;
    
    u8 output = (~m_channel4.lfsr) & 0x01;
    return output ? (m_channel4.volume / 15.0f) : 0.0f;
}

void APU::triggerChannel1() {
    // Channel is enabled if DAC is on (envelope bits 3-7 are not all zero)
    if ((m_channel1.envelope & 0xF8) == 0) {
        m_channel1.enabled = false;
        m_nr52 &= ~0x01;
        return;
    }
    
    m_channel1.enabled = true;
    m_nr52 |= 0x01;
    
    // Length counter edge case: if length is 0, reload it
    // If length enable is set and we're on an odd frame, decrement after reload
    if (m_channel1.lengthCounter == 0) {
        m_channel1.lengthCounter = 64;
        // If length is enabled and we're on an odd frame (about to clock length)
        if ((m_channel1.freqHigh & 0x40) && (m_frameSequencer & 1)) {
            m_channel1.lengthCounter--;
        }
    }
    
    m_channel1.frequencyCounter = 0;
    m_channel1.volume = (m_channel1.envelope >> 4) & 0x0F;
    m_channel1.envelopePeriod = m_channel1.envelope & 0x07;
    m_channel1.envelopeCounter = m_channel1.envelopePeriod;
    m_channel1.envelopeIncrease = (m_channel1.envelope & 0x08) != 0;
    
    // Sweep initialization
    m_channel1.sweepPeriod = (m_channel1.sweep >> 4) & 0x07;
    if (m_channel1.sweepPeriod == 0) {
        m_channel1.sweepPeriod = 8;
    }
    m_channel1.sweepCounter = m_channel1.sweepPeriod;
    m_channel1.sweepShift = m_channel1.sweep & 0x07;
    // Bit 3 set = decrease (subtract), bit 3 clear = increase (add)
    m_channel1.sweepIncrease = (m_channel1.sweep & 0x08) != 0;
    
    // Perform initial sweep overflow check if sweep shift is non-zero
    if (m_channel1.sweepShift > 0) {
        u16 newFreq = m_channel1.frequency + (m_channel1.frequency >> m_channel1.sweepShift);
        if (newFreq > 2047) {
            m_channel1.enabled = false;
            m_nr52 &= ~0x01;
        }
    }
}

void APU::triggerChannel2() {
    // Channel is enabled if DAC is on (envelope bits 3-7 are not all zero)
    if ((m_channel2.envelope & 0xF8) == 0) {
        m_channel2.enabled = false;
        m_nr52 &= ~0x02;
        return;
    }
    
    m_channel2.enabled = true;
    m_nr52 |= 0x02;
    
    // Length counter edge case: if length is 0, reload it
    if (m_channel2.lengthCounter == 0) {
        m_channel2.lengthCounter = 64;
        // If length is enabled and we're on an odd frame
        if ((m_channel2.freqHigh & 0x40) && (m_frameSequencer & 1)) {
            m_channel2.lengthCounter--;
        }
    }
    
    m_channel2.frequencyCounter = 0;
    m_channel2.volume = (m_channel2.envelope >> 4) & 0x0F;
    m_channel2.envelopePeriod = m_channel2.envelope & 0x07;
    m_channel2.envelopeCounter = m_channel2.envelopePeriod;
    m_channel2.envelopeIncrease = (m_channel2.envelope & 0x08) != 0;
}

void APU::triggerChannel3() {
    // Channel 3 is enabled if the DAC is on (bit 7 of NR30)
    if (!(m_channel3.onOff & 0x80)) {
        m_channel3.enabled = false;
        m_nr52 &= ~0x04;
        return;
    }
    
    m_channel3.enabled = true;
    m_nr52 |= 0x04;
    
    // Length counter edge case: if length is 0, reload it
    if (m_channel3.lengthCounter == 0) {
        m_channel3.lengthCounter = 256;
        // If length is enabled and we're on an odd frame
        if ((m_channel3.freqHigh & 0x40) && (m_frameSequencer & 1)) {
            m_channel3.lengthCounter--;
        }
    }
    
    m_channel3.frequencyCounter = 0;
    m_channel3.wavePosition = 0;
}

void APU::triggerChannel4() {
    // Channel is enabled if DAC is on (envelope bits 3-7 are not all zero)
    if ((m_channel4.envelope & 0xF8) == 0) {
        m_channel4.enabled = false;
        m_nr52 &= ~0x08;
        return;
    }
    
    m_channel4.enabled = true;
    m_nr52 |= 0x08;
    
    // Length counter edge case: if length is 0, reload it
    if (m_channel4.lengthCounter == 0) {
        m_channel4.lengthCounter = 64;
        // If length is enabled and we're on an odd frame
        if ((m_channel4.control & 0x40) && (m_frameSequencer & 1)) {
            m_channel4.lengthCounter--;
        }
    }
    
    m_channel4.clockCounter = 0;
    // LFSR should be initialized to all 1s (0x7FFF for 15-bit)
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
        case 0xFF14: {
            bool wasLengthEnabled = (m_channel1.freqHigh & 0x40) != 0;
            m_channel1.freqHigh = value;
            m_channel1.frequency = (m_channel1.frequency & 0xFF) | ((value & 0x07) << 8);
            
            // Edge case: enabling length on odd frame decrements length
            bool lengthEnabled = (value & 0x40) != 0;
            if (!wasLengthEnabled && lengthEnabled && (m_frameSequencer & 1) && m_channel1.lengthCounter > 0) {
                m_channel1.lengthCounter--;
                if (m_channel1.lengthCounter == 0) {
                    m_channel1.enabled = false;
                    m_nr52 &= ~0x01;
                }
            }
            
            if (value & 0x80) {
                triggerChannel1();
            }
            break;
        }
        
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
        case 0xFF19: {
            bool wasLengthEnabled = (m_channel2.freqHigh & 0x40) != 0;
            m_channel2.freqHigh = value;
            m_channel2.frequency = (m_channel2.frequency & 0xFF) | ((value & 0x07) << 8);
            
            // Edge case: enabling length on odd frame decrements length
            bool lengthEnabled = (value & 0x40) != 0;
            if (!wasLengthEnabled && lengthEnabled && (m_frameSequencer & 1) && m_channel2.lengthCounter > 0) {
                m_channel2.lengthCounter--;
                if (m_channel2.lengthCounter == 0) {
                    m_channel2.enabled = false;
                    m_nr52 &= ~0x02;
                }
            }
            
            if (value & 0x80) {
                triggerChannel2();
            }
            break;
        }
        
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
        case 0xFF1E: {
            bool wasLengthEnabled = (m_channel3.freqHigh & 0x40) != 0;
            m_channel3.freqHigh = value;
            m_channel3.frequency = (m_channel3.frequency & 0xFF) | ((value & 0x07) << 8);
            
            // Edge case: enabling length on odd frame decrements length
            bool lengthEnabled = (value & 0x40) != 0;
            if (!wasLengthEnabled && lengthEnabled && (m_frameSequencer & 1) && m_channel3.lengthCounter > 0) {
                m_channel3.lengthCounter--;
                if (m_channel3.lengthCounter == 0) {
                    m_channel3.enabled = false;
                    m_nr52 &= ~0x04;
                }
            }
            
            if (value & 0x80) {
                triggerChannel3();
            }
            break;
        }
        
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
        case 0xFF23: {
            bool wasLengthEnabled = (m_channel4.control & 0x40) != 0;
            m_channel4.control = value;
            
            // Edge case: enabling length on odd frame decrements length
            bool lengthEnabled = (value & 0x40) != 0;
            if (!wasLengthEnabled && lengthEnabled && (m_frameSequencer & 1) && m_channel4.lengthCounter > 0) {
                m_channel4.lengthCounter--;
                if (m_channel4.lengthCounter == 0) {
                    m_channel4.enabled = false;
                    m_nr52 &= ~0x08;
                }
            }
            
            if (value & 0x80) {
                triggerChannel4();
            }
            break;
        }
        
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

