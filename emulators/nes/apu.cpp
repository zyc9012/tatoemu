#include "apu.h"
#include "cpu.h"
#include "memory.h"
#include "cartridge.h"
#include <cstring>
#include <cmath>
#include <algorithm>

namespace nes {

// ============================================================
// Static Tables
// ============================================================

// Length counter lookup table
const u8 APU::LengthCounter::LENGTH_TABLE[32] = {
    10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14, 12, 26, 14,
    12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
};

// Pulse duty cycle sequences
// Each entry represents 8 steps; 1 = high, 0 = low
const u8 APU::PulseChannel::DUTY_TABLE[4][8] = {
    {0, 1, 0, 0, 0, 0, 0, 0},  // 12.5%
    {0, 1, 1, 0, 0, 0, 0, 0},  // 25%
    {0, 1, 1, 1, 1, 0, 0, 0},  // 50%
    {1, 0, 0, 1, 1, 1, 1, 1}   // 25% negated (75%)
};

// Triangle channel sequence (32 steps)
const u8 APU::TriangleChannel::TRIANGLE_TABLE[32] = {
    15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
};

// Noise channel period lookup table (NTSC)
const u16 APU::NoiseChannel::NOISE_PERIOD_TABLE[16] = {
    4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
};

// Noise channel period lookup table (PAL)
const u16 APU::NoiseChannel::NOISE_PERIOD_TABLE_PAL[16] = {
    4, 8, 14, 30, 60, 88, 118, 148, 188, 236, 354, 472, 708, 944, 1890, 3778
};

// DMC rate lookup table (NTSC) - in CPU cycles
const u16 APU::DMCChannel::DMC_RATE_TABLE[16] = {
    428, 380, 340, 320, 286, 254, 226, 214, 190, 160, 142, 128, 106, 84, 72, 54
};

// DMC rate lookup table (PAL)
const u16 APU::DMCChannel::DMC_RATE_TABLE_PAL[16] = {
    398, 354, 316, 298, 276, 236, 210, 198, 176, 148, 132, 118, 98, 78, 66, 50
};

// Frame counter step timings (in CPU cycles from reset)
const u32 APU::FrameCounter::STEP_CYCLES_4[4] = {7457, 14913, 22371, 29829};
const u32 APU::FrameCounter::STEP_CYCLES_5[5] = {7457, 14913, 22371, 29829, 37281};

// ============================================================
// Envelope Implementation
// ============================================================

void APU::Envelope::reset() {
    start = false;
    loop = false;
    constantVolume = false;
    dividerPeriod = 0;
    dividerCounter = 0;
    decayLevel = 0;
}

void APU::Envelope::clock() {
    if (start) {
        start = false;
        decayLevel = 15;
        dividerCounter = dividerPeriod;
    } else {
        if (dividerCounter == 0) {
            dividerCounter = dividerPeriod;
            if (decayLevel > 0) {
                decayLevel--;
            } else if (loop) {
                decayLevel = 15;
            }
        } else {
            dividerCounter--;
        }
    }
}

u8 APU::Envelope::volume() const {
    return constantVolume ? dividerPeriod : decayLevel;
}

// ============================================================
// Sweep Unit Implementation
// ============================================================

void APU::Sweep::reset() {
    enabled = false;
    negate = false;
    reload = false;
    dividerPeriod = 0;
    dividerCounter = 0;
    shiftCount = 0;
    pulseChannel = false;
}

u16 APU::Sweep::targetPeriod(u16 currentPeriod) const {
    u16 changeAmount = currentPeriod >> shiftCount;
    
    if (negate) {
        // Pulse 1 uses one's complement (subtract + 1)
        // Pulse 2 uses two's complement (just subtract)
        if (pulseChannel) {
            return currentPeriod - changeAmount - 1;
        } else {
            return currentPeriod - changeAmount;
        }
    } else {
        return currentPeriod + changeAmount;
    }
}

bool APU::Sweep::isMuting(u16 timerPeriod) const {
    // Mute if period < 8 or target period > $7FF
    if (timerPeriod < 8) return true;
    if (!negate && targetPeriod(timerPeriod) > 0x7FF) return true;
    return false;
}

void APU::Sweep::clock(u16& timerPeriod) {
    // Calculate target period
    u16 target = targetPeriod(timerPeriod);
    
    // Clock divider
    if (dividerCounter == 0 && enabled && shiftCount > 0 && !isMuting(timerPeriod)) {
        // Only update period if target is valid
        if (target <= 0x7FF) {
            timerPeriod = target;
        }
    }
    
    // Reload divider or decrement
    if (dividerCounter == 0 || reload) {
        dividerCounter = dividerPeriod;
        reload = false;
    } else {
        dividerCounter--;
    }
}

// ============================================================
// Length Counter Implementation
// ============================================================

void APU::LengthCounter::reset() {
    enabled = false;
    halt = false;
    counter = 0;
}

void APU::LengthCounter::clock() {
    if (counter > 0 && !halt) {
        counter--;
    }
}

void APU::LengthCounter::load(u8 index) {
    if (enabled) {
        counter = LENGTH_TABLE[index & 0x1F];
    }
}

// ============================================================
// Pulse Channel Implementation
// ============================================================

void APU::PulseChannel::reset() {
    timerPeriod = 0;
    timerCounter = 0;
    dutyMode = 0;
    sequencerStep = 0;
    envelope.reset();
    sweep.reset();
    lengthCounter.reset();
}

void APU::PulseChannel::clockTimer() {
    if (timerCounter == 0) {
        timerCounter = timerPeriod;
        // Advance sequencer (wraps 0-7)
        sequencerStep = (sequencerStep + 1) & 7;
    } else {
        timerCounter--;
    }
}

void APU::PulseChannel::clockSweep() {
    sweep.clock(timerPeriod);
}

u8 APU::PulseChannel::output() const {
    // Check all silencing conditions
    if (lengthCounter.isZero()) return 0;
    if (sweep.isMuting(timerPeriod)) return 0;
    if (DUTY_TABLE[dutyMode][sequencerStep] == 0) return 0;
    
    return envelope.volume();
}

void APU::PulseChannel::writeControl(u8 value) {
    dutyMode = (value >> 6) & 0x03;
    lengthCounter.halt = (value & 0x20) != 0;
    envelope.loop = (value & 0x20) != 0;
    envelope.constantVolume = (value & 0x10) != 0;
    envelope.dividerPeriod = value & 0x0F;
}

void APU::PulseChannel::writeSweep(u8 value) {
    sweep.enabled = (value & 0x80) != 0;
    sweep.dividerPeriod = (value >> 4) & 0x07;
    sweep.negate = (value & 0x08) != 0;
    sweep.shiftCount = value & 0x07;
    sweep.reload = true;
}

void APU::PulseChannel::writeTimerLow(u8 value) {
    timerPeriod = (timerPeriod & 0x700) | value;
}

void APU::PulseChannel::writeTimerHigh(u8 value) {
    timerPeriod = (timerPeriod & 0x0FF) | ((value & 0x07) << 8);
    lengthCounter.load((value >> 3) & 0x1F);
    
    // Reset sequencer and envelope
    sequencerStep = 0;
    envelope.start = true;
}

// ============================================================
// Triangle Channel Implementation
// ============================================================

void APU::TriangleChannel::reset() {
    timerPeriod = 0;
    timerCounter = 0;
    sequencerStep = 0;
    linearCounterReload = false;
    linearCounterPeriod = 0;
    linearCounter = 0;
    controlFlag = false;
    lengthCounter.reset();
}

void APU::TriangleChannel::clockTimer() {
    if (timerCounter == 0) {
        timerCounter = timerPeriod;
        // Only advance if length counter and linear counter are non-zero
        if (lengthCounter.counter > 0 && linearCounter > 0) {
            sequencerStep = (sequencerStep + 1) & 31;
        }
    } else {
        timerCounter--;
    }
}

void APU::TriangleChannel::clockLinearCounter() {
    if (linearCounterReload) {
        linearCounter = linearCounterPeriod;
    } else if (linearCounter > 0) {
        linearCounter--;
    }
    
    if (!controlFlag) {
        linearCounterReload = false;
    }
}

u8 APU::TriangleChannel::output() const {
    // Note: Triangle channel doesn't actually silence at period < 2
    // It just plays ultrasonic frequencies that get filtered out
    // However, we do silence at very low periods to avoid aliasing
    if (timerPeriod < 2) return 7; // Return midpoint to avoid pops
    
    return TRIANGLE_TABLE[sequencerStep];
}

void APU::TriangleChannel::writeControl(u8 value) {
    controlFlag = (value & 0x80) != 0;
    lengthCounter.halt = controlFlag;
    linearCounterPeriod = value & 0x7F;
}

void APU::TriangleChannel::writeTimerLow(u8 value) {
    timerPeriod = (timerPeriod & 0x700) | value;
}

void APU::TriangleChannel::writeTimerHigh(u8 value) {
    timerPeriod = (timerPeriod & 0x0FF) | ((value & 0x07) << 8);
    lengthCounter.load((value >> 3) & 0x1F);
    linearCounterReload = true;
}

// ============================================================
// Noise Channel Implementation
// ============================================================

void APU::NoiseChannel::reset() {
    timerPeriod = NOISE_PERIOD_TABLE[0];
    timerCounter = 0;
    shiftRegister = 1; // Must be non-zero
    mode = false;
    envelope.reset();
    lengthCounter.reset();
}

void APU::NoiseChannel::clockTimer() {
    if (timerCounter == 0) {
        timerCounter = timerPeriod;
        
        // Clock the shift register
        // Feedback bit is XOR of bit 0 and bit 1 (mode=0) or bit 6 (mode=1)
        u8 feedbackBit = mode ? 6 : 1;
        u16 feedback = (shiftRegister & 1) ^ ((shiftRegister >> feedbackBit) & 1);
        shiftRegister >>= 1;
        shiftRegister |= (feedback << 14);
    } else {
        timerCounter--;
    }
}

u8 APU::NoiseChannel::output() const {
    if (lengthCounter.isZero()) return 0;
    if (shiftRegister & 1) return 0; // Bit 0 set = silence
    
    return envelope.volume();
}

void APU::NoiseChannel::writeControl(u8 value) {
    lengthCounter.halt = (value & 0x20) != 0;
    envelope.loop = (value & 0x20) != 0;
    envelope.constantVolume = (value & 0x10) != 0;
    envelope.dividerPeriod = value & 0x0F;
}

void APU::NoiseChannel::writePeriod(u8 value) {
    mode = (value & 0x80) != 0;
    timerPeriod = NOISE_PERIOD_TABLE[value & 0x0F];
}

void APU::NoiseChannel::writeLength(u8 value) {
    lengthCounter.load((value >> 3) & 0x1F);
    envelope.start = true;
}

// ============================================================
// DMC Channel Implementation
// ============================================================

void APU::DMCChannel::reset() {
    timerPeriod = DMC_RATE_TABLE[0];
    timerCounter = timerPeriod;
    sampleAddress = 0xC000;
    sampleLength = 0;
    addressStart = 0xC000;
    lengthStart = 1;
    sampleBuffer = 0;
    sampleBufferEmpty = true;
    shiftRegister = 0;
    bitsRemaining = 0;
    outputLevel = 0;
    silenceFlag = true;
    irqEnabled = false;
    loopFlag = false;
    irqFlag = false;
}

void APU::DMCChannel::clockTimer() {
    if (timerCounter == 0) {
        timerCounter = timerPeriod;
        
        // Clock output unit
        if (!silenceFlag) {
            // Delta modulation: add or subtract 2 from output level
            if (shiftRegister & 1) {
                if (outputLevel <= 125) {
                    outputLevel += 2;
                }
            } else {
                if (outputLevel >= 2) {
                    outputLevel -= 2;
                }
            }
        }
        
        // Shift register
        shiftRegister >>= 1;
        bitsRemaining--;
        
        // Check if output cycle complete
        if (bitsRemaining == 0) {
            bitsRemaining = 8;
            
            if (sampleBufferEmpty) {
                silenceFlag = true;
            } else {
                silenceFlag = false;
                shiftRegister = sampleBuffer;
                sampleBufferEmpty = true;
                
                // Try to fill buffer
                readSampleByte();
            }
        }
    } else {
        timerCounter--;
    }
}

void APU::DMCChannel::startSample() {
    sampleAddress = addressStart;
    sampleLength = lengthStart;
}

void APU::DMCChannel::readSampleByte() {
    if (sampleLength > 0 && sampleBufferEmpty && memory) {
        // Read sample byte from memory
        // Note: This causes CPU stall in real hardware (DMA)
        // We simplify by doing immediate read
        sampleBuffer = memory->cpuRead(sampleAddress);
        sampleBufferEmpty = false;
        
        // Advance address (wrap from $FFFF to $8000)
        sampleAddress++;
        if (sampleAddress == 0) {
            sampleAddress = 0x8000;
        }
        
        // Decrement length
        sampleLength--;
        
        // Check for sample completion
        if (sampleLength == 0) {
            if (loopFlag) {
                startSample();
            } else if (irqEnabled) {
                irqFlag = true;
            }
        }
    }
}

void APU::DMCChannel::writeControl(u8 value) {
    irqEnabled = (value & 0x80) != 0;
    loopFlag = (value & 0x40) != 0;
    timerPeriod = DMC_RATE_TABLE[value & 0x0F];
    
    if (!irqEnabled) {
        irqFlag = false;
    }
}

void APU::DMCChannel::writeDirectLoad(u8 value) {
    outputLevel = value & 0x7F;
}

void APU::DMCChannel::writeAddress(u8 value) {
    // Address = $C000 + (A * 64)
    addressStart = 0xC000 + (value * 64);
}

void APU::DMCChannel::writeLength(u8 value) {
    // Length = (L * 16) + 1 bytes
    lengthStart = (value * 16) + 1;
}

// ============================================================
// Frame Counter Implementation
// ============================================================

void APU::FrameCounter::reset() {
    cycleCounter = 0;
    step = 0;
    mode = false;
    irqInhibit = false;
    irqFlag = false;
    resetDelay = false;
    resetCounter = 0;
}

// ============================================================
// APU Main Implementation
// ============================================================

APU::APU()
    : m_cpu(nullptr)
    , m_memory(nullptr)
    , m_cartridge(nullptr)
    , m_audioDevice(nullptr)
    , m_totalCycles(0)
    , m_lastFrameCycle(0)
    , m_oddCycle(false)
    , m_sampleCounter(0)
    , m_cyclesPerSample(0)
    , m_highPass1(0)
    , m_highPass2(0)
    , m_lowPass(0) {
    buildMixerTables();
    m_sampleBuffer.reserve(SAMPLE_BUFFER_SIZE);
}

void APU::setMemory(Memory* memory) {
    m_memory = memory;
    m_dmc.memory = memory;
}

void APU::setSampleRate(u32 sampleRate) {
    m_sampleRate = sampleRate;
    // NTSC CPU runs at 1.789773 MHz
    m_cyclesPerSample = 1789773.0 / sampleRate;
}

void APU::buildMixerTables() {
    // Build pulse mixer table (non-linear mixing)
    // output = 95.52 / (8128.0 / (pulse1 + pulse2) + 100)
    m_pulseTable[0] = 0;
    for (int i = 1; i < 31; i++) {
        m_pulseTable[i] = 95.52f / (8128.0f / i + 100.0f);
    }
    
    // Build TND (triangle, noise, DMC) mixer table
    // output = 163.67 / (24329.0 / (3*triangle + 2*noise + dmc) + 100)
    m_tndTable[0] = 0;
    for (int i = 1; i < 203; i++) {
        m_tndTable[i] = 163.67f / (24329.0f / i + 100.0f);
    }
}

void APU::reset() {
    m_pulse1.reset();
    m_pulse2.reset();
    m_triangle.reset();
    m_noise.reset();
    m_dmc.reset();
    m_frameCounter.reset();
    
    m_pulse1.isPulse1 = true;
    m_pulse1.sweep.pulseChannel = true;
    m_pulse2.isPulse1 = false;
    m_pulse2.sweep.pulseChannel = false;
    
    m_totalCycles = 0;
    m_lastFrameCycle = 0;
    m_oddCycle = false;
    m_sampleCounter = 0;
    
    m_highPass1 = 0;
    m_highPass2 = 0;
    m_lowPass = 0;
    
    m_sampleBuffer.clear();
    
    // Disable all channels
    writeRegister(0x4015, 0);
}

void APU::step(u32 cpuCycles, double gameSpeed) {
    for (u32 i = 0; i < cpuCycles; i++) {
        m_totalCycles++;
        m_oddCycle = !m_oddCycle;
        
        // Triangle channel timer clocks every CPU cycle
        m_triangle.clockTimer();
        
        // DMC timer also clocks every CPU cycle (rate table values are in CPU cycles)
        m_dmc.clockTimer();
        
        // Clock expansion audio (VRC6, etc.) every CPU cycle
        if (m_cartridge) {
            m_cartridge->clockAudio();
        }
        
        // Pulse and Noise channels clock every other CPU cycle (APU cycle)
        if (m_oddCycle) {
            m_pulse1.clockTimer();
            m_pulse2.clockTimer();
            m_noise.clockTimer();
        }
        
        // Handle frame counter reset delay
        if (m_frameCounter.resetDelay) {
            m_frameCounter.resetCounter--;
            if (m_frameCounter.resetCounter == 0) {
                m_frameCounter.resetDelay = false;
                m_frameCounter.cycleCounter = 0;
                m_frameCounter.step = 0;
                m_lastFrameCycle = m_totalCycles;
                
                // 5-step mode clocks immediately
                if (m_frameCounter.mode) {
                    clockQuarterFrame();
                    clockHalfFrame();
                }
            }
        }
        
        // Frame counter
        m_frameCounter.cycleCounter++;
        
        // Check for frame counter steps
        bool clockQuarter = false;
        bool clockHalf = false;
        
        if (!m_frameCounter.mode) {
            // 4-step mode
            switch (m_frameCounter.step) {
                case 0:
                    if (m_frameCounter.cycleCounter >= FrameCounter::STEP_CYCLES_4[0]) {
                        clockQuarter = true;
                        m_frameCounter.step++;
                    }
                    break;
                case 1:
                    if (m_frameCounter.cycleCounter >= FrameCounter::STEP_CYCLES_4[1]) {
                        clockQuarter = true;
                        clockHalf = true;
                        m_frameCounter.step++;
                    }
                    break;
                case 2:
                    if (m_frameCounter.cycleCounter >= FrameCounter::STEP_CYCLES_4[2]) {
                        clockQuarter = true;
                        m_frameCounter.step++;
                    }
                    break;
                case 3:
                    if (m_frameCounter.cycleCounter >= FrameCounter::STEP_CYCLES_4[3]) {
                        clockQuarter = true;
                        clockHalf = true;
                        m_frameCounter.cycleCounter = 0;
                        m_frameCounter.step = 0;
                    }
                    break;
            }
        } else {
            // 5-step mode
            switch (m_frameCounter.step) {
                case 0:
                    if (m_frameCounter.cycleCounter >= FrameCounter::STEP_CYCLES_5[0]) {
                        clockQuarter = true;
                        m_frameCounter.step++;
                    }
                    break;
                case 1:
                    if (m_frameCounter.cycleCounter >= FrameCounter::STEP_CYCLES_5[1]) {
                        clockQuarter = true;
                        clockHalf = true;
                        m_frameCounter.step++;
                    }
                    break;
                case 2:
                    if (m_frameCounter.cycleCounter >= FrameCounter::STEP_CYCLES_5[2]) {
                        clockQuarter = true;
                        m_frameCounter.step++;
                    }
                    break;
                case 3:
                    if (m_frameCounter.cycleCounter >= FrameCounter::STEP_CYCLES_5[3]) {
                        // Nothing happens on step 4 in 5-step mode
                        m_frameCounter.step++;
                    }
                    break;
                case 4:
                    if (m_frameCounter.cycleCounter >= FrameCounter::STEP_CYCLES_5[4]) {
                        clockQuarter = true;
                        clockHalf = true;
                        m_frameCounter.cycleCounter = 0;
                        m_frameCounter.step = 0;
                    }
                    break;
            }
        }
        
        if (clockQuarter) {
            clockQuarterFrame();
        }
        if (clockHalf) {
            clockHalfFrame();
        }
        
        // Generate audio sample
        m_sampleCounter += 1.0;
        if (m_sampleCounter >= m_cyclesPerSample * gameSpeed) {
            m_sampleCounter -= m_cyclesPerSample * gameSpeed;
            float sample = mix();
            outputSample(sample);
        }
    }
}

void APU::clockQuarterFrame() {
    // Clock envelopes
    m_pulse1.clockEnvelope();
    m_pulse2.clockEnvelope();
    m_noise.clockEnvelope();
    
    // Clock triangle linear counter
    m_triangle.clockLinearCounter();
}

void APU::clockHalfFrame() {
    // Clock length counters
    m_pulse1.clockLength();
    m_pulse2.clockLength();
    m_triangle.clockLength();
    m_noise.clockLength();
    
    // Clock sweep units
    m_pulse1.clockSweep();
    m_pulse2.clockSweep();
}

float APU::mix() {
    // Get channel outputs
    u8 pulse1 = m_pulse1.output();
    u8 pulse2 = m_pulse2.output();
    u8 triangle = m_triangle.output();
    u8 noise = m_noise.output();
    u8 dmc = m_dmc.output();
    
    // Non-linear mixing using lookup tables
    float pulseOut = m_pulseTable[pulse1 + pulse2];
    float tndOut = m_tndTable[3 * triangle + 2 * noise + dmc];
    
    float sample = pulseOut + tndOut;
    
    // Add expansion audio (VRC6, VRC7, etc.)
    if (m_cartridge && m_cartridge->hasExpansionAudio()) {
        float expansionAudio = m_cartridge->getExpansionAudio();
        // Mix expansion audio - typically about 50% of total output
        sample += expansionAudio;
    }
    
    // Apply high-pass filter at 90 Hz (removes DC offset)
    float filtered = sample - m_highPass1;
    m_highPass1 += filtered * (1.0f - HIGH_PASS_90HZ);
    sample = filtered;
    
    // Apply high-pass filter at 440 Hz
    filtered = sample - m_highPass2;
    m_highPass2 += filtered * (1.0f - HIGH_PASS_440HZ);
    sample = filtered;
    
    // Apply low-pass filter at 14 kHz (removes aliasing)
    sample = m_lowPass + (sample - m_lowPass) * LOW_PASS_14KHZ;
    m_lowPass = sample;
    
    return sample * m_volume;
}

void APU::outputSample(float sample) {
    // Output stereo (duplicate mono sample to both channels)
    m_sampleBuffer.push_back(sample);  // Left
    m_sampleBuffer.push_back(sample);  // Right
    
    // When buffer is full, send to audio device
    if (m_sampleBuffer.size() >= SAMPLE_BUFFER_SIZE && m_audioDevice) {
        m_audioDevice->writeSamples(m_sampleBuffer.data(), m_sampleBuffer.size() * sizeof(float));
        m_sampleBuffer.clear();
    }
}

u8 APU::readStatus() {
    u8 result = 0;
    
    // Bits 0-3: Length counter > 0
    if (!m_pulse1.lengthCounter.isZero()) result |= 0x01;
    if (!m_pulse2.lengthCounter.isZero()) result |= 0x02;
    if (!m_triangle.lengthCounter.isZero()) result |= 0x04;
    if (!m_noise.lengthCounter.isZero()) result |= 0x08;
    
    // Bit 4: DMC bytes remaining > 0
    if (m_dmc.sampleLength > 0) result |= 0x10;
    
    // Bit 5: unused (reads as 0)
    
    // Bit 6: Frame interrupt flag
    if (m_frameCounter.irqFlag) result |= 0x40;
    
    // Bit 7: DMC interrupt flag
    if (m_dmc.irqFlag) result |= 0x80;
    
    // Reading status clears frame interrupt flag
    m_frameCounter.irqFlag = false;
    
    return result;
}

void APU::writeRegister(u16 address, u8 value) {
    switch (address) {
        // Pulse 1
        case 0x4000:
            m_pulse1.writeControl(value);
            break;
        case 0x4001:
            m_pulse1.writeSweep(value);
            break;
        case 0x4002:
            m_pulse1.writeTimerLow(value);
            break;
        case 0x4003:
            m_pulse1.writeTimerHigh(value);
            break;
        
        // Pulse 2
        case 0x4004:
            m_pulse2.writeControl(value);
            break;
        case 0x4005:
            m_pulse2.writeSweep(value);
            break;
        case 0x4006:
            m_pulse2.writeTimerLow(value);
            break;
        case 0x4007:
            m_pulse2.writeTimerHigh(value);
            break;
        
        // Triangle
        case 0x4008:
            m_triangle.writeControl(value);
            break;
        case 0x4009:
            // Unused
            break;
        case 0x400A:
            m_triangle.writeTimerLow(value);
            break;
        case 0x400B:
            m_triangle.writeTimerHigh(value);
            break;
        
        // Noise
        case 0x400C:
            m_noise.writeControl(value);
            break;
        case 0x400D:
            // Unused
            break;
        case 0x400E:
            m_noise.writePeriod(value);
            break;
        case 0x400F:
            m_noise.writeLength(value);
            break;
        
        // DMC
        case 0x4010:
            m_dmc.writeControl(value);
            break;
        case 0x4011:
            m_dmc.writeDirectLoad(value);
            break;
        case 0x4012:
            m_dmc.writeAddress(value);
            break;
        case 0x4013:
            m_dmc.writeLength(value);
            break;
        
        // Status
        case 0x4015: {
            // Enable/disable channels
            m_pulse1.lengthCounter.enabled = (value & 0x01) != 0;
            m_pulse2.lengthCounter.enabled = (value & 0x02) != 0;
            m_triangle.lengthCounter.enabled = (value & 0x04) != 0;
            m_noise.lengthCounter.enabled = (value & 0x08) != 0;
            
            // Disable channels: clear length counter
            if (!m_pulse1.lengthCounter.enabled) {
                m_pulse1.lengthCounter.counter = 0;
            }
            if (!m_pulse2.lengthCounter.enabled) {
                m_pulse2.lengthCounter.counter = 0;
            }
            if (!m_triangle.lengthCounter.enabled) {
                m_triangle.lengthCounter.counter = 0;
            }
            if (!m_noise.lengthCounter.enabled) {
                m_noise.lengthCounter.counter = 0;
            }
            
            // DMC
            m_dmc.irqFlag = false; // Clear DMC IRQ flag
            if (value & 0x10) {
                // Enable DMC: restart if bytes remaining = 0
                if (m_dmc.sampleLength == 0) {
                    m_dmc.startSample();
                    m_dmc.readSampleByte();
                }
            } else {
                // Disable DMC: clear bytes remaining
                m_dmc.sampleLength = 0;
            }
            break;
        }
        
        // Frame counter
        case 0x4017:
            m_frameCounter.mode = (value & 0x80) != 0;
            m_frameCounter.irqInhibit = (value & 0x40) != 0;
            
            if (m_frameCounter.irqInhibit) {
                m_frameCounter.irqFlag = false;
            }
            
            // Reset is delayed by 3-4 cycles
            m_frameCounter.resetDelay = true;
            m_frameCounter.resetCounter = m_oddCycle ? 4 : 3;
            break;
    }
}

void APU::saveState(Buffer* buf) {
    // Save pulse 1
    buffer_write(buf, &m_pulse1.timerPeriod, sizeof(m_pulse1.timerPeriod));
    buffer_write(buf, &m_pulse1.timerCounter, sizeof(m_pulse1.timerCounter));
    buffer_write(buf, &m_pulse1.dutyMode, sizeof(m_pulse1.dutyMode));
    buffer_write(buf, &m_pulse1.sequencerStep, sizeof(m_pulse1.sequencerStep));
    buffer_write(buf, &m_pulse1.envelope, sizeof(m_pulse1.envelope));
    buffer_write(buf, &m_pulse1.sweep, sizeof(m_pulse1.sweep));
    buffer_write(buf, &m_pulse1.lengthCounter, sizeof(m_pulse1.lengthCounter));
    
    // Save pulse 2
    buffer_write(buf, &m_pulse2.timerPeriod, sizeof(m_pulse2.timerPeriod));
    buffer_write(buf, &m_pulse2.timerCounter, sizeof(m_pulse2.timerCounter));
    buffer_write(buf, &m_pulse2.dutyMode, sizeof(m_pulse2.dutyMode));
    buffer_write(buf, &m_pulse2.sequencerStep, sizeof(m_pulse2.sequencerStep));
    buffer_write(buf, &m_pulse2.envelope, sizeof(m_pulse2.envelope));
    buffer_write(buf, &m_pulse2.sweep, sizeof(m_pulse2.sweep));
    buffer_write(buf, &m_pulse2.lengthCounter, sizeof(m_pulse2.lengthCounter));
    
    // Save triangle
    buffer_write(buf, &m_triangle.timerPeriod, sizeof(m_triangle.timerPeriod));
    buffer_write(buf, &m_triangle.timerCounter, sizeof(m_triangle.timerCounter));
    buffer_write(buf, &m_triangle.sequencerStep, sizeof(m_triangle.sequencerStep));
    buffer_write(buf, &m_triangle.linearCounterReload, sizeof(m_triangle.linearCounterReload));
    buffer_write(buf, &m_triangle.linearCounterPeriod, sizeof(m_triangle.linearCounterPeriod));
    buffer_write(buf, &m_triangle.linearCounter, sizeof(m_triangle.linearCounter));
    buffer_write(buf, &m_triangle.controlFlag, sizeof(m_triangle.controlFlag));
    buffer_write(buf, &m_triangle.lengthCounter, sizeof(m_triangle.lengthCounter));
    
    // Save noise
    buffer_write(buf, &m_noise.timerPeriod, sizeof(m_noise.timerPeriod));
    buffer_write(buf, &m_noise.timerCounter, sizeof(m_noise.timerCounter));
    buffer_write(buf, &m_noise.shiftRegister, sizeof(m_noise.shiftRegister));
    buffer_write(buf, &m_noise.mode, sizeof(m_noise.mode));
    buffer_write(buf, &m_noise.envelope, sizeof(m_noise.envelope));
    buffer_write(buf, &m_noise.lengthCounter, sizeof(m_noise.lengthCounter));
    
    // Save DMC
    buffer_write(buf, &m_dmc.timerPeriod, sizeof(m_dmc.timerPeriod));
    buffer_write(buf, &m_dmc.timerCounter, sizeof(m_dmc.timerCounter));
    buffer_write(buf, &m_dmc.sampleAddress, sizeof(m_dmc.sampleAddress));
    buffer_write(buf, &m_dmc.sampleLength, sizeof(m_dmc.sampleLength));
    buffer_write(buf, &m_dmc.addressStart, sizeof(m_dmc.addressStart));
    buffer_write(buf, &m_dmc.lengthStart, sizeof(m_dmc.lengthStart));
    buffer_write(buf, &m_dmc.sampleBuffer, sizeof(m_dmc.sampleBuffer));
    buffer_write(buf, &m_dmc.sampleBufferEmpty, sizeof(m_dmc.sampleBufferEmpty));
    buffer_write(buf, &m_dmc.shiftRegister, sizeof(m_dmc.shiftRegister));
    buffer_write(buf, &m_dmc.bitsRemaining, sizeof(m_dmc.bitsRemaining));
    buffer_write(buf, &m_dmc.outputLevel, sizeof(m_dmc.outputLevel));
    buffer_write(buf, &m_dmc.silenceFlag, sizeof(m_dmc.silenceFlag));
    buffer_write(buf, &m_dmc.irqEnabled, sizeof(m_dmc.irqEnabled));
    buffer_write(buf, &m_dmc.loopFlag, sizeof(m_dmc.loopFlag));
    buffer_write(buf, &m_dmc.irqFlag, sizeof(m_dmc.irqFlag));
    
    // Save frame counter
    buffer_write(buf, &m_frameCounter, sizeof(m_frameCounter));
    
    // Save timing state
    buffer_write(buf, &m_totalCycles, sizeof(m_totalCycles));
    buffer_write(buf, &m_lastFrameCycle, sizeof(m_lastFrameCycle));
    buffer_write(buf, &m_oddCycle, sizeof(m_oddCycle));
}

void APU::loadState(Buffer* buf) {
    // Load pulse 1
    buffer_read(buf, &m_pulse1.timerPeriod, sizeof(m_pulse1.timerPeriod));
    buffer_read(buf, &m_pulse1.timerCounter, sizeof(m_pulse1.timerCounter));
    buffer_read(buf, &m_pulse1.dutyMode, sizeof(m_pulse1.dutyMode));
    buffer_read(buf, &m_pulse1.sequencerStep, sizeof(m_pulse1.sequencerStep));
    buffer_read(buf, &m_pulse1.envelope, sizeof(m_pulse1.envelope));
    buffer_read(buf, &m_pulse1.sweep, sizeof(m_pulse1.sweep));
    buffer_read(buf, &m_pulse1.lengthCounter, sizeof(m_pulse1.lengthCounter));
    m_pulse1.isPulse1 = true;
    m_pulse1.sweep.pulseChannel = true;
    
    // Load pulse 2
    buffer_read(buf, &m_pulse2.timerPeriod, sizeof(m_pulse2.timerPeriod));
    buffer_read(buf, &m_pulse2.timerCounter, sizeof(m_pulse2.timerCounter));
    buffer_read(buf, &m_pulse2.dutyMode, sizeof(m_pulse2.dutyMode));
    buffer_read(buf, &m_pulse2.sequencerStep, sizeof(m_pulse2.sequencerStep));
    buffer_read(buf, &m_pulse2.envelope, sizeof(m_pulse2.envelope));
    buffer_read(buf, &m_pulse2.sweep, sizeof(m_pulse2.sweep));
    buffer_read(buf, &m_pulse2.lengthCounter, sizeof(m_pulse2.lengthCounter));
    m_pulse2.isPulse1 = false;
    m_pulse2.sweep.pulseChannel = false;
    
    // Load triangle
    buffer_read(buf, &m_triangle.timerPeriod, sizeof(m_triangle.timerPeriod));
    buffer_read(buf, &m_triangle.timerCounter, sizeof(m_triangle.timerCounter));
    buffer_read(buf, &m_triangle.sequencerStep, sizeof(m_triangle.sequencerStep));
    buffer_read(buf, &m_triangle.linearCounterReload, sizeof(m_triangle.linearCounterReload));
    buffer_read(buf, &m_triangle.linearCounterPeriod, sizeof(m_triangle.linearCounterPeriod));
    buffer_read(buf, &m_triangle.linearCounter, sizeof(m_triangle.linearCounter));
    buffer_read(buf, &m_triangle.controlFlag, sizeof(m_triangle.controlFlag));
    buffer_read(buf, &m_triangle.lengthCounter, sizeof(m_triangle.lengthCounter));
    
    // Load noise
    buffer_read(buf, &m_noise.timerPeriod, sizeof(m_noise.timerPeriod));
    buffer_read(buf, &m_noise.timerCounter, sizeof(m_noise.timerCounter));
    buffer_read(buf, &m_noise.shiftRegister, sizeof(m_noise.shiftRegister));
    buffer_read(buf, &m_noise.mode, sizeof(m_noise.mode));
    buffer_read(buf, &m_noise.envelope, sizeof(m_noise.envelope));
    buffer_read(buf, &m_noise.lengthCounter, sizeof(m_noise.lengthCounter));
    
    // Load DMC
    buffer_read(buf, &m_dmc.timerPeriod, sizeof(m_dmc.timerPeriod));
    buffer_read(buf, &m_dmc.timerCounter, sizeof(m_dmc.timerCounter));
    buffer_read(buf, &m_dmc.sampleAddress, sizeof(m_dmc.sampleAddress));
    buffer_read(buf, &m_dmc.sampleLength, sizeof(m_dmc.sampleLength));
    buffer_read(buf, &m_dmc.addressStart, sizeof(m_dmc.addressStart));
    buffer_read(buf, &m_dmc.lengthStart, sizeof(m_dmc.lengthStart));
    buffer_read(buf, &m_dmc.sampleBuffer, sizeof(m_dmc.sampleBuffer));
    buffer_read(buf, &m_dmc.sampleBufferEmpty, sizeof(m_dmc.sampleBufferEmpty));
    buffer_read(buf, &m_dmc.shiftRegister, sizeof(m_dmc.shiftRegister));
    buffer_read(buf, &m_dmc.bitsRemaining, sizeof(m_dmc.bitsRemaining));
    buffer_read(buf, &m_dmc.outputLevel, sizeof(m_dmc.outputLevel));
    buffer_read(buf, &m_dmc.silenceFlag, sizeof(m_dmc.silenceFlag));
    buffer_read(buf, &m_dmc.irqEnabled, sizeof(m_dmc.irqEnabled));
    buffer_read(buf, &m_dmc.loopFlag, sizeof(m_dmc.loopFlag));
    buffer_read(buf, &m_dmc.irqFlag, sizeof(m_dmc.irqFlag));
    m_dmc.memory = m_memory;
    
    // Load frame counter
    buffer_read(buf, &m_frameCounter, sizeof(m_frameCounter));
    
    // Load timing state
    buffer_read(buf, &m_totalCycles, sizeof(m_totalCycles));
    buffer_read(buf, &m_lastFrameCycle, sizeof(m_lastFrameCycle));
    buffer_read(buf, &m_oddCycle, sizeof(m_oddCycle));
    
    // Reset audio state
    m_sampleCounter = 0;
    m_highPass1 = 0;
    m_highPass2 = 0;
    m_lowPass = 0;
    m_sampleBuffer.clear();
}

} // namespace nes
