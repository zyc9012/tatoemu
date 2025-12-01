#include "mapper024.h"
#include "../consts.h"
#include <cstring>

namespace nes {

// VRC6 Audio Pulse Channel
void VRC6Pulse::reset() {
    volume = 0;
    duty = 0;
    period = 0;
    timer = 0;
    step = 0;
    enabled = false;
    mode = false;
}

void VRC6Pulse::clockTimer() {
    if (!enabled) return;
    
    if (timer == 0) {
        timer = period;
        // Advance step (16 steps)
        step = (step + 1) & 0x0F;
    } else {
        timer--;
    }
}

u8 VRC6Pulse::output() const {
    if (!enabled) return 0;
    if (period < 1) return 0;  // Prevent ultrasonic frequencies
    
    // VRC6 pulse has 16 steps with variable duty cycle
    // duty value 0-7 means output high for (duty+1) steps out of 16
    // When mode bit is set, output is always the volume (no duty cycle)
    if (mode) {
        return volume;
    }
    
    // step goes 0-15, output high if step <= duty
    if (step <= duty) {
        return volume;
    }
    return 0;
}

// VRC6 Audio Sawtooth Channel
void VRC6Sawtooth::reset() {
    accumRate = 0;
    period = 0;
    timer = 0;
    accumulator = 0;
    step = 0;
    enabled = false;
}

void VRC6Sawtooth::clockTimer() {
    if (!enabled) return;
    
    if (timer == 0) {
        timer = period;
        
        // Accumulator is clocked every 2 steps
        step++;
        if ((step & 1) == 0) {
            // Add rate to accumulator (on even steps)
            accumulator += accumRate;
        }
        
        // Reset on step 14
        if (step >= 14) {
            step = 0;
            accumulator = 0;
        }
    } else {
        timer--;
    }
}

u8 VRC6Sawtooth::output() const {
    if (!enabled) return 0;
    if (period < 1) return 0;
    
    // Output is the top 5 bits of the accumulator
    return (accumulator >> 3) & 0x1F;
}

// Mapper 24: VRC6a
Mapper024::Mapper024(Cartridge* cartridge)
    : Mapper(cartridge)
    , m_prgBank16k(0)
    , m_prgBank8k(0)
    , m_mirrorMode(MirrorMode::VERTICAL)
    , m_irqLatch(0)
    , m_irqCounter(0)
    , m_irqPrescaler(0)
    , m_irqPrescalerCounter(0)
    , m_irqEnable(false)
    , m_irqEnableOnAck(false)
    , m_irqMode(false)
    , m_audioHalt(false) {
    std::memset(m_chrBank, 0, sizeof(m_chrBank));
    std::memset(m_prgBankOffset, 0, sizeof(m_prgBankOffset));
    std::memset(m_chrBankOffset, 0, sizeof(m_chrBankOffset));
    m_vrcPulse1.reset();
    m_vrcPulse2.reset();
    m_vrcSaw.reset();
}

void Mapper024::reset() {
    m_prgBank16k = 0;
    m_prgBank8k = 0;
    std::memset(m_chrBank, 0, sizeof(m_chrBank));
    m_mirrorMode = MirrorMode::VERTICAL;
    m_irqLatch = 0;
    m_irqCounter = 0;
    m_irqPrescaler = 0;
    m_irqPrescalerCounter = 0;
    m_irqEnable = false;
    m_irqEnableOnAck = false;
    m_irqMode = false;
    m_irqActive = false;
    
    // Audio reset
    m_vrcPulse1.reset();
    m_vrcPulse2.reset();
    m_vrcSaw.reset();
    m_audioHalt = false;
    
    updateBanks();
}

void Mapper024::updateBanks() {
    const auto& prg = m_cartridge->getPRG();
    const auto& chr = m_cartridge->getCHR();
    
    u32 prgBanks16k = prg.size() / 0x4000;
    u32 prgBanks8k = prg.size() / 0x2000;
    u32 chrBanks1k = chr.size() / 0x400;
    if (chrBanks1k == 0) chrBanks1k = 8;
    
    // PRG: 16KB at $8000, 8KB at $C000, fixed 8KB at $E000
    m_prgBankOffset[0] = (m_prgBank16k % prgBanks16k) * 0x4000;
    m_prgBankOffset[1] = m_prgBankOffset[0] + 0x2000;
    m_prgBankOffset[2] = (m_prgBank8k % prgBanks8k) * 0x2000;
    m_prgBankOffset[3] = (prgBanks8k - 1) * 0x2000;
    
    // CHR: 1KB banks
    for (int i = 0; i < 8; i++) {
        m_chrBankOffset[i] = (m_chrBank[i] % chrBanks1k) * 0x400;
    }
}

u8 Mapper024::cpuRead(u16 address) {
    if (address >= 0x6000 && address < 0x8000) {
        return m_cartridge->getPRGRAM()[address & 0x1FFF];
    } else if (address >= 0x8000) {
        u8 bank = (address - 0x8000) / 0x2000;
        u16 offset = address & 0x1FFF;
        return m_cartridge->getPRG()[m_prgBankOffset[bank] + offset];
    }
    return 0;
}

void Mapper024::cpuWrite(u16 address, u8 value) {
    if (address >= 0x6000 && address < 0x8000) {
        m_cartridge->getPRGRAM()[address & 0x1FFF] = value;
        return;
    }
    
    switch (address) {
        case 0x8000: case 0x8001: case 0x8002: case 0x8003:
            m_prgBank16k = value & 0x0F;
            updateBanks();
            break;
            
        // VRC6 Audio Pulse 1
        case 0x9000:
            m_vrcPulse1.volume = value & 0x0F;
            m_vrcPulse1.duty = (value >> 4) & 0x07;
            m_vrcPulse1.mode = (value & 0x80) != 0;
            break;
        case 0x9001:
            m_vrcPulse1.period = (m_vrcPulse1.period & 0xF00) | value;
            break;
        case 0x9002:
            m_vrcPulse1.period = (m_vrcPulse1.period & 0x0FF) | ((value & 0x0F) << 8);
            m_vrcPulse1.enabled = (value & 0x80) != 0;
            break;
            
        // VRC6 Audio Pulse 2
        case 0xA000:
            m_vrcPulse2.volume = value & 0x0F;
            m_vrcPulse2.duty = (value >> 4) & 0x07;
            m_vrcPulse2.mode = (value & 0x80) != 0;
            break;
        case 0xA001:
            m_vrcPulse2.period = (m_vrcPulse2.period & 0xF00) | value;
            break;
        case 0xA002:
            m_vrcPulse2.period = (m_vrcPulse2.period & 0x0FF) | ((value & 0x0F) << 8);
            m_vrcPulse2.enabled = (value & 0x80) != 0;
            break;
            
        // VRC6 Audio Sawtooth
        case 0xB000:
            m_vrcSaw.accumRate = value & 0x3F;
            break;
        case 0xB001:
            m_vrcSaw.period = (m_vrcSaw.period & 0xF00) | value;
            break;
        case 0xB002:
            m_vrcSaw.period = (m_vrcSaw.period & 0x0FF) | ((value & 0x0F) << 8);
            m_vrcSaw.enabled = (value & 0x80) != 0;
            break;
            
        case 0xB003:
            // Bits 0-1: PPU banking style (ignored for now)
            // Bits 2-3: Mirroring
            switch ((value >> 2) & 0x03) {
                case 0: m_mirrorMode = MirrorMode::VERTICAL; break;
                case 1: m_mirrorMode = MirrorMode::HORIZONTAL; break;
                case 2: m_mirrorMode = MirrorMode::SINGLE_SCREEN_A; break;
                case 3: m_mirrorMode = MirrorMode::SINGLE_SCREEN_B; break;
            }
            // Bit 4: Audio halt
            m_audioHalt = (value & 0x10) != 0;
            break;
            
        case 0xC000: case 0xC001: case 0xC002: case 0xC003:
            m_prgBank8k = value & 0x1F;
            updateBanks();
            break;
            
        case 0xD000:
            m_chrBank[0] = value;
            updateBanks();
            break;
        case 0xD001:
            m_chrBank[1] = value;
            updateBanks();
            break;
        case 0xD002:
            m_chrBank[2] = value;
            updateBanks();
            break;
        case 0xD003:
            m_chrBank[3] = value;
            updateBanks();
            break;
        case 0xE000:
            m_chrBank[4] = value;
            updateBanks();
            break;
        case 0xE001:
            m_chrBank[5] = value;
            updateBanks();
            break;
        case 0xE002:
            m_chrBank[6] = value;
            updateBanks();
            break;
        case 0xE003:
            m_chrBank[7] = value;
            updateBanks();
            break;
            
        case 0xF000:
            m_irqLatch = value;
            break;
        case 0xF001:
            m_irqEnableOnAck = (value & 0x01) != 0;
            m_irqEnable = (value & 0x02) != 0;
            m_irqMode = (value & 0x04) != 0;
            if (m_irqEnable) {
                m_irqCounter = m_irqLatch;
                m_irqPrescalerCounter = 341;
            }
            m_irqActive = false;
            break;
        case 0xF002:
            m_irqEnable = m_irqEnableOnAck;
            m_irqActive = false;
            break;
    }
}

void Mapper024::clockAudio() {
    if (m_audioHalt) return;
    
    m_vrcPulse1.clockTimer();
    m_vrcPulse2.clockTimer();
    m_vrcSaw.clockTimer();
}

float Mapper024::getAudioOutput() const {
    if (m_audioHalt) return 0.0f;
    
    // Get raw outputs (0-15 for pulse, 0-31 for saw)
    u8 pulse1 = m_vrcPulse1.output();
    u8 pulse2 = m_vrcPulse2.output();
    u8 saw = m_vrcSaw.output();
    
    // Mix channels - VRC6 has louder output than internal APU
    // Scale to roughly 0.0-0.5 range (VRC6 is about 50% of total output when combined)
    // Pulse channels are 4-bit (0-15), saw is 5-bit (0-31)
    float pulseOut = (pulse1 + pulse2) / 30.0f;  // Max 30, normalize
    float sawOut = saw / 31.0f;
    
    // VRC6 mixing is roughly equal weighted between pulses and saw
    float output = (pulseOut * 0.5f + sawOut * 0.5f) * 0.5f;
    
    return output;
}

u8 Mapper024::readCHR(u16 address) {
    u8 bank = address / 0x400;
    u16 offset = address & 0x3FF;
    return m_cartridge->getCHR()[m_chrBankOffset[bank] + offset];
}

void Mapper024::writeCHR(u16 address, u8 value) {
    u8 bank = address / 0x400;
    u16 offset = address & 0x3FF;
    m_cartridge->getCHR()[m_chrBankOffset[bank] + offset] = value;
}

MirrorMode Mapper024::getMirrorMode() const {
    return m_mirrorMode;
}

void Mapper024::scanlineCounter() {
    if (!m_irqEnable) return;
    
    if (m_irqMode) {
        // Cycle mode
        m_irqPrescalerCounter--;
        if (m_irqPrescalerCounter <= 0) {
            m_irqPrescalerCounter = 341;
            if (m_irqCounter == 0xFF) {
                m_irqCounter = m_irqLatch;
                m_irqActive = true;
            } else {
                m_irqCounter++;
            }
        }
    } else {
        // Scanline mode
        if (m_irqCounter == 0xFF) {
            m_irqCounter = m_irqLatch;
            m_irqActive = true;
        } else {
            m_irqCounter++;
        }
    }
}

void Mapper024::saveState(std::ofstream& file) const {
    file.write(reinterpret_cast<const char*>(&m_prgBank16k), sizeof(m_prgBank16k));
    file.write(reinterpret_cast<const char*>(&m_prgBank8k), sizeof(m_prgBank8k));
    file.write(reinterpret_cast<const char*>(m_chrBank), sizeof(m_chrBank));
    file.write(reinterpret_cast<const char*>(&m_mirrorMode), sizeof(m_mirrorMode));
    file.write(reinterpret_cast<const char*>(&m_irqLatch), sizeof(m_irqLatch));
    file.write(reinterpret_cast<const char*>(&m_irqCounter), sizeof(m_irqCounter));
    file.write(reinterpret_cast<const char*>(&m_irqPrescaler), sizeof(m_irqPrescaler));
    file.write(reinterpret_cast<const char*>(&m_irqPrescalerCounter), sizeof(m_irqPrescalerCounter));
    file.write(reinterpret_cast<const char*>(&m_irqEnable), sizeof(m_irqEnable));
    file.write(reinterpret_cast<const char*>(&m_irqEnableOnAck), sizeof(m_irqEnableOnAck));
    file.write(reinterpret_cast<const char*>(&m_irqMode), sizeof(m_irqMode));
    // VRC6 Audio state
    file.write(reinterpret_cast<const char*>(&m_vrcPulse1), sizeof(m_vrcPulse1));
    file.write(reinterpret_cast<const char*>(&m_vrcPulse2), sizeof(m_vrcPulse2));
    file.write(reinterpret_cast<const char*>(&m_vrcSaw), sizeof(m_vrcSaw));
    file.write(reinterpret_cast<const char*>(&m_audioHalt), sizeof(m_audioHalt));
}

void Mapper024::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(&m_prgBank16k), sizeof(m_prgBank16k));
    file.read(reinterpret_cast<char*>(&m_prgBank8k), sizeof(m_prgBank8k));
    file.read(reinterpret_cast<char*>(m_chrBank), sizeof(m_chrBank));
    file.read(reinterpret_cast<char*>(&m_mirrorMode), sizeof(m_mirrorMode));
    file.read(reinterpret_cast<char*>(&m_irqLatch), sizeof(m_irqLatch));
    file.read(reinterpret_cast<char*>(&m_irqCounter), sizeof(m_irqCounter));
    file.read(reinterpret_cast<char*>(&m_irqPrescaler), sizeof(m_irqPrescaler));
    file.read(reinterpret_cast<char*>(&m_irqPrescalerCounter), sizeof(m_irqPrescalerCounter));
    file.read(reinterpret_cast<char*>(&m_irqEnable), sizeof(m_irqEnable));
    file.read(reinterpret_cast<char*>(&m_irqEnableOnAck), sizeof(m_irqEnableOnAck));
    file.read(reinterpret_cast<char*>(&m_irqMode), sizeof(m_irqMode));
    // VRC6 Audio state
    file.read(reinterpret_cast<char*>(&m_vrcPulse1), sizeof(m_vrcPulse1));
    file.read(reinterpret_cast<char*>(&m_vrcPulse2), sizeof(m_vrcPulse2));
    file.read(reinterpret_cast<char*>(&m_vrcSaw), sizeof(m_vrcSaw));
    file.read(reinterpret_cast<char*>(&m_audioHalt), sizeof(m_audioHalt));
    updateBanks();
}

} // namespace nes

