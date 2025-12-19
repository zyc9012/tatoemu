#include "mapper024.h"
#include "../consts.h"
#include <cstring>
#include <cmath>

namespace nes {

// ============================================================
// VRC6 Pulse Channel Implementation
// ============================================================

void VRC6Pulse::reset() {
    m_volume = 0;
    m_dutyCycle = 0;
    m_ignoreDuty = false;
    m_frequency = 1;
    m_enabled = false;
    m_timer = 1;
    m_step = 0;
    m_frequencyShift = 0;
}

void VRC6Pulse::writeReg(u16 addr, u8 value) {
    switch (addr & 0x03) {
        case 0:
            // $9000/$A000: Volume/Duty Control
            // D3-D0: Volume
            // D6-D4: Duty cycle (0-7)
            // D7: Ignore duty (constant volume output)
            m_volume = value & 0x0F;
            m_dutyCycle = (value >> 4) & 0x07;
            m_ignoreDuty = (value & 0x80) != 0;
            break;
            
        case 1:
            // $9001/$A001: Frequency Low
            m_frequency = (m_frequency & 0x0F00) | value;
            break;
            
        case 2:
            // $9002/$A002: Frequency High + Enable
            // D3-D0: Frequency high 4 bits
            // D7: Enable channel
            m_frequency = (m_frequency & 0x00FF) | ((value & 0x0F) << 8);
            m_enabled = (value & 0x80) != 0;
            if (!m_enabled) {
                m_step = 0;
            }
            break;
    }
}

void VRC6Pulse::clock() {
    if (m_enabled) {
        m_timer--;
        if (m_timer == 0) {
            m_step = (m_step + 1) & 0x0F;
            // Reload timer with frequency (shifted if speed control is active)
            m_timer = (m_frequency >> m_frequencyShift) + 1;
        }
    }
}

u8 VRC6Pulse::getVolume() const {
    if (!m_enabled) {
        return 0;
    }
    if (m_ignoreDuty) {
        // Output constant volume
        return m_volume;
    }
    // Output volume when step <= duty cycle, otherwise 0
    // Step goes from 0-15, duty from 0-7
    // When step <= duty, output is high
    return (m_step <= m_dutyCycle) ? m_volume : 0;
}

void VRC6Pulse::saveState(std::ofstream& file) const {
    file.write(reinterpret_cast<const char*>(&m_volume), sizeof(m_volume));
    file.write(reinterpret_cast<const char*>(&m_dutyCycle), sizeof(m_dutyCycle));
    file.write(reinterpret_cast<const char*>(&m_ignoreDuty), sizeof(m_ignoreDuty));
    file.write(reinterpret_cast<const char*>(&m_frequency), sizeof(m_frequency));
    file.write(reinterpret_cast<const char*>(&m_enabled), sizeof(m_enabled));
    file.write(reinterpret_cast<const char*>(&m_timer), sizeof(m_timer));
    file.write(reinterpret_cast<const char*>(&m_step), sizeof(m_step));
    file.write(reinterpret_cast<const char*>(&m_frequencyShift), sizeof(m_frequencyShift));
}

void VRC6Pulse::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(&m_volume), sizeof(m_volume));
    file.read(reinterpret_cast<char*>(&m_dutyCycle), sizeof(m_dutyCycle));
    file.read(reinterpret_cast<char*>(&m_ignoreDuty), sizeof(m_ignoreDuty));
    file.read(reinterpret_cast<char*>(&m_frequency), sizeof(m_frequency));
    file.read(reinterpret_cast<char*>(&m_enabled), sizeof(m_enabled));
    file.read(reinterpret_cast<char*>(&m_timer), sizeof(m_timer));
    file.read(reinterpret_cast<char*>(&m_step), sizeof(m_step));
    file.read(reinterpret_cast<char*>(&m_frequencyShift), sizeof(m_frequencyShift));
}

// ============================================================
// VRC6 Sawtooth Channel Implementation
// ============================================================

void VRC6Saw::reset() {
    m_accumulatorRate = 0;
    m_accumulator = 0;
    m_frequency = 1;
    m_enabled = false;
    m_timer = 1;
    m_step = 0;
    m_frequencyShift = 0;
}

void VRC6Saw::writeReg(u16 addr, u8 value) {
    switch (addr & 0x03) {
        case 0:
            // $B000: Accumulator Rate
            // D5-D0: Rate value added to accumulator
            m_accumulatorRate = value & 0x3F;
            break;
            
        case 1:
            // $B001: Frequency Low
            m_frequency = (m_frequency & 0x0F00) | value;
            break;
            
        case 2:
            // $B002: Frequency High + Enable
            // D3-D0: Frequency high 4 bits
            // D7: Enable channel
            m_frequency = (m_frequency & 0x00FF) | ((value & 0x0F) << 8);
            m_enabled = (value & 0x80) != 0;
            if (!m_enabled) {
                // If E is clear, the accumulator is forced to zero
                m_accumulator = 0;
                // The phase can be mostly reset by clearing and setting E
                // Clearing E does not reset the frequency divider
                m_step = 0;
            }
            break;
    }
}

void VRC6Saw::clock() {
    if (m_enabled) {
        m_timer--;
        if (m_timer == 0) {
            m_step = (m_step + 1) % 14;
            m_timer = (m_frequency >> m_frequencyShift) + 1;
            
            if (m_step == 0) {
                // Reset accumulator at start of cycle
                m_accumulator = 0;
            } else if ((m_step & 0x01) == 0) {
                // Add rate on even steps (2, 4, 6, 8, 10, 12)
                m_accumulator += m_accumulatorRate;
            }
        }
    }
}

u8 VRC6Saw::getVolume() const {
    if (!m_enabled) {
        return 0;
    }
    // Output the high 5 bits of the accumulator
    return m_accumulator >> 3;
}

void VRC6Saw::saveState(std::ofstream& file) const {
    file.write(reinterpret_cast<const char*>(&m_accumulatorRate), sizeof(m_accumulatorRate));
    file.write(reinterpret_cast<const char*>(&m_accumulator), sizeof(m_accumulator));
    file.write(reinterpret_cast<const char*>(&m_frequency), sizeof(m_frequency));
    file.write(reinterpret_cast<const char*>(&m_enabled), sizeof(m_enabled));
    file.write(reinterpret_cast<const char*>(&m_timer), sizeof(m_timer));
    file.write(reinterpret_cast<const char*>(&m_step), sizeof(m_step));
    file.write(reinterpret_cast<const char*>(&m_frequencyShift), sizeof(m_frequencyShift));
}

void VRC6Saw::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(&m_accumulatorRate), sizeof(m_accumulatorRate));
    file.read(reinterpret_cast<char*>(&m_accumulator), sizeof(m_accumulator));
    file.read(reinterpret_cast<char*>(&m_frequency), sizeof(m_frequency));
    file.read(reinterpret_cast<char*>(&m_enabled), sizeof(m_enabled));
    file.read(reinterpret_cast<char*>(&m_timer), sizeof(m_timer));
    file.read(reinterpret_cast<char*>(&m_step), sizeof(m_step));
    file.read(reinterpret_cast<char*>(&m_frequencyShift), sizeof(m_frequencyShift));
}

// ============================================================
// VRC6 Audio Controller Implementation
// ============================================================

void VRC6Audio::reset() {
    m_pulse1.reset();
    m_pulse2.reset();
    m_saw.reset();
    m_haltAudio = false;
    m_lastOutput = 0;
}

void VRC6Audio::writeRegister(u16 addr, u8 value) {
    switch (addr & 0xF003) {
        case 0x9000: case 0x9001: case 0x9002:
            m_pulse1.writeReg(addr, value);
            break;
            
        case 0x9003: {
            // Audio control register
            // D0: Halt audio (freeze all timers)
            // D1-D2: Frequency shift (0, 4, or 8 bits)
            m_haltAudio = (value & 0x01) != 0;
            u8 frequencyShift = 0;
            if (value & 0x04) {
                frequencyShift = 8;
            } else if (value & 0x02) {
                frequencyShift = 4;
            }
            m_pulse1.setFrequencyShift(frequencyShift);
            m_pulse2.setFrequencyShift(frequencyShift);
            m_saw.setFrequencyShift(frequencyShift);
            break;
        }
            
        case 0xA000: case 0xA001: case 0xA002:
            m_pulse2.writeReg(addr, value);
            break;
            
        case 0xB000: case 0xB001: case 0xB002:
            m_saw.writeReg(addr, value);
            break;
    }
}

void VRC6Audio::clock() {
    if (!m_haltAudio) {
        m_pulse1.clock();
        m_pulse2.clock();
        m_saw.clock();
    }
}

float VRC6Audio::getOutput() const {
    // Combine outputs from all three channels
    // Pulse channels output 0-15, saw outputs 0-31
    // Total max output: 15 + 15 + 31 = 61
    s32 outputLevel = m_pulse1.getVolume() + m_pulse2.getVolume() + m_saw.getVolume();
    
    // Normalize to roughly match internal APU levels
    // The VRC6 audio is typically mixed at about 1/6 the volume of the internal APU
    // Pulse output is 0-15, Saw is 0-31
    // We normalize to a -1.0 to 1.0 range, scaled for proper mixing
    return static_cast<float>(outputLevel) / 61.0f * 0.5f;
}

void VRC6Audio::saveState(std::ofstream& file) const {
    m_pulse1.saveState(file);
    m_pulse2.saveState(file);
    m_saw.saveState(file);
    file.write(reinterpret_cast<const char*>(&m_haltAudio), sizeof(m_haltAudio));
    file.write(reinterpret_cast<const char*>(&m_lastOutput), sizeof(m_lastOutput));
}

void VRC6Audio::loadState(std::ifstream& file) {
    m_pulse1.loadState(file);
    m_pulse2.loadState(file);
    m_saw.loadState(file);
    file.read(reinterpret_cast<char*>(&m_haltAudio), sizeof(m_haltAudio));
    file.read(reinterpret_cast<char*>(&m_lastOutput), sizeof(m_lastOutput));
}

// ============================================================
// Mapper 024 (VRC6a) Implementation
// ============================================================

Mapper024::Mapper024(Cartridge* cartridge)
    : Mapper(cartridge) {
}

void Mapper024::reset() {
    m_prgBank16k = 0;
    m_prgBank8k = 0;
    m_bankingMode = 0;
    m_mirrorMode = m_cartridge->getBaseMirrorMode();
    m_irqLatch = 0;
    m_irqCounter = 0;
    m_irqEnable = false;
    m_irqEnableOnAck = false;
    m_irqCycleMode = false;
    m_irqPrescaler = 341;
    m_irqActive = false;
    
    std::memset(m_chrBank, 0, sizeof(m_chrBank));
    std::memset(m_chrBankOffset, 0, sizeof(m_chrBankOffset));
    std::memset(m_prgBankOffset, 0, sizeof(m_prgBankOffset));
    
    m_audio.reset();
    updateBanks();
}

void Mapper024::updateBanks() {
    const auto& prg = m_cartridge->getPRG();
    const auto& chr = m_cartridge->getCHR();
    
    u32 prgBanks8k = prg.size() / 0x2000;
    u32 chrBanks1k = chr.size() / 0x400;
    if (chrBanks1k == 0) chrBanks1k = 8;  // CHR RAM
    
    // PRG banking:
    // $8000-$BFFF: 16KB switchable (uses m_prgBank16k, selects 2 consecutive 8KB banks)
    // $C000-$DFFF: 8KB switchable (uses m_prgBank8k)
    // $E000-$FFFF: Fixed to last 8KB bank
    u8 bank16k = (m_prgBank16k & 0x0F) * 2;  // Convert to 8KB bank index
    m_prgBankOffset[0] = (bank16k % prgBanks8k) * 0x2000;
    m_prgBankOffset[1] = ((bank16k + 1) % prgBanks8k) * 0x2000;
    m_prgBankOffset[2] = ((m_prgBank8k & 0x1F) % prgBanks8k) * 0x2000;
    m_prgBankOffset[3] = (prgBanks8k - 1) * 0x2000;  // Fixed to last bank
    
    // CHR banking - depends on banking mode bits 0-1 and bit 5
    // Bit 5: 0 = 1KB pages, 1 = 2KB pages (mask & or)
    u8 mask = (m_bankingMode & 0x20) ? 0xFE : 0xFF;
    u8 orMask = (m_bankingMode & 0x20) ? 1 : 0;
    
    switch (m_bankingMode & 0x03) {
        case 0:
            // Mode 0: 8 x 1KB banks
            m_chrBankOffset[0] = (m_chrBank[0] % chrBanks1k) * 0x400;
            m_chrBankOffset[1] = (m_chrBank[1] % chrBanks1k) * 0x400;
            m_chrBankOffset[2] = (m_chrBank[2] % chrBanks1k) * 0x400;
            m_chrBankOffset[3] = (m_chrBank[3] % chrBanks1k) * 0x400;
            m_chrBankOffset[4] = (m_chrBank[4] % chrBanks1k) * 0x400;
            m_chrBankOffset[5] = (m_chrBank[5] % chrBanks1k) * 0x400;
            m_chrBankOffset[6] = (m_chrBank[6] % chrBanks1k) * 0x400;
            m_chrBankOffset[7] = (m_chrBank[7] % chrBanks1k) * 0x400;
            break;
            
        case 1:
            // Mode 1: 4 x 2KB banks (registers 0-3 each select 2 consecutive 1KB banks)
            m_chrBankOffset[0] = ((m_chrBank[0] & mask) % chrBanks1k) * 0x400;
            m_chrBankOffset[1] = (((m_chrBank[0] & mask) | orMask) % chrBanks1k) * 0x400;
            m_chrBankOffset[2] = ((m_chrBank[1] & mask) % chrBanks1k) * 0x400;
            m_chrBankOffset[3] = (((m_chrBank[1] & mask) | orMask) % chrBanks1k) * 0x400;
            m_chrBankOffset[4] = ((m_chrBank[2] & mask) % chrBanks1k) * 0x400;
            m_chrBankOffset[5] = (((m_chrBank[2] & mask) | orMask) % chrBanks1k) * 0x400;
            m_chrBankOffset[6] = ((m_chrBank[3] & mask) % chrBanks1k) * 0x400;
            m_chrBankOffset[7] = (((m_chrBank[3] & mask) | orMask) % chrBanks1k) * 0x400;
            break;
            
        case 2:
        case 3:
            // Mode 2/3: 4 x 1KB + 2 x 2KB (first 4KB uses 1KB banks, second 4KB uses 2KB)
            m_chrBankOffset[0] = (m_chrBank[0] % chrBanks1k) * 0x400;
            m_chrBankOffset[1] = (m_chrBank[1] % chrBanks1k) * 0x400;
            m_chrBankOffset[2] = (m_chrBank[2] % chrBanks1k) * 0x400;
            m_chrBankOffset[3] = (m_chrBank[3] % chrBanks1k) * 0x400;
            m_chrBankOffset[4] = ((m_chrBank[4] & mask) % chrBanks1k) * 0x400;
            m_chrBankOffset[5] = (((m_chrBank[4] & mask) | orMask) % chrBanks1k) * 0x400;
            m_chrBankOffset[6] = ((m_chrBank[5] & mask) % chrBanks1k) * 0x400;
            m_chrBankOffset[7] = (((m_chrBank[5] & mask) | orMask) % chrBanks1k) * 0x400;
            break;
    }
}

void Mapper024::updateMirroring() {
    // Mirroring is controlled by $B003
    // Bit 4: 0 = regular CIRAM nametables, 1 = CHR ROM nametables (not supported here)
    // Bits 2-3 and other bits control mirroring pattern
    
    // For regular nametables (CIRAM), use the mirroring control bits
    // The mirroring mode depends on bits 0-3 and bit 5
    u8 mirrorBits = m_bankingMode & 0x2F;
    
    switch (mirrorBits) {
        case 0x20:
        case 0x27:
            m_mirrorMode = MirrorMode::VERTICAL;
            break;
            
        case 0x23:
        case 0x24:
            m_mirrorMode = MirrorMode::HORIZONTAL;
            break;
            
        case 0x28:
        case 0x2F:
            m_mirrorMode = MirrorMode::SINGLE_SCREEN_A;
            break;
            
        case 0x2B:
        case 0x2C:
            m_mirrorMode = MirrorMode::SINGLE_SCREEN_B;
            break;
            
        default:
            // For other values, use bits 0-2 to determine nametable mapping
            // This maps individual nametables based on CHR register bits
            switch (m_bankingMode & 0x07) {
                case 0:
                case 6:
                case 7:
                    // Nametables based on chrBank[6] bit 0 for NT0/1, chrBank[7] bit 0 for NT2/3
                    // This creates a horizontal-like mirror pattern
                    if ((m_chrBank[6] & 0x01) == (m_chrBank[7] & 0x01)) {
                        m_mirrorMode = (m_chrBank[6] & 0x01) ? MirrorMode::SINGLE_SCREEN_B : MirrorMode::SINGLE_SCREEN_A;
                    } else {
                        m_mirrorMode = MirrorMode::HORIZONTAL;
                    }
                    break;
                    
                case 1:
                case 5:
                    // Nametables based on chrBank[4-7] individual bits
                    // Four-screen-like behavior simplified to closest match
                    m_mirrorMode = MirrorMode::VERTICAL;
                    break;
                    
                case 2:
                case 3:
                case 4:
                    // Diagonal-like pattern
                    if ((m_chrBank[6] & 0x01) == (m_chrBank[7] & 0x01)) {
                        m_mirrorMode = (m_chrBank[6] & 0x01) ? MirrorMode::SINGLE_SCREEN_B : MirrorMode::SINGLE_SCREEN_A;
                    } else {
                        m_mirrorMode = MirrorMode::VERTICAL;
                    }
                    break;
            }
            break;
    }
}

u8 Mapper024::cpuRead(u16 address) {
    if (address >= 0x6000 && address < 0x8000) {
        // PRG RAM at $6000-$7FFF (if enabled by banking mode bit 7)
        if (m_bankingMode & 0x80) {
            return m_cartridge->getPRGRAM()[address & 0x1FFF];
        }
        return 0;  // Open bus if disabled
    } else if (address >= 0x8000) {
        // PRG ROM
        u8 bank = (address - 0x8000) / 0x2000;
        u16 offset = address & 0x1FFF;
        return m_cartridge->getPRG()[m_prgBankOffset[bank] + offset];
    }
    return 0;
}

void Mapper024::cpuWrite(u16 address, u8 value) {
    if (address >= 0x6000 && address < 0x8000) {
        // PRG RAM at $6000-$7FFF (if enabled)
        if (m_bankingMode & 0x80) {
            m_cartridge->getPRGRAM()[address & 0x1FFF] = value;
        }
        return;
    }
    
    // VRC6a addressing: use A0 and A1 directly
    // For VRC6b (mapper 26), A0 and A1 would be swapped
    switch (address & 0xF003) {
        // PRG Bank 0 (16KB at $8000-$BFFF)
        case 0x8000: case 0x8001: case 0x8002: case 0x8003:
            m_prgBank16k = value & 0x0F;
            updateBanks();
            break;
            
        // Audio registers $9000-$9003
        case 0x9000: case 0x9001: case 0x9002: case 0x9003:
            m_audio.writeRegister(address, value);
            break;
            
        // Audio registers $A000-$A002
        case 0xA000: case 0xA001: case 0xA002:
            m_audio.writeRegister(address, value);
            break;
            
        // Audio registers $B000-$B002
        case 0xB000: case 0xB001: case 0xB002:
            m_audio.writeRegister(address, value);
            break;
            
        // Banking mode / PPU banking control
        case 0xB003:
            m_bankingMode = value;
            updateMirroring();
            updateBanks();
            break;
            
        // PRG Bank 1 (8KB at $C000-$DFFF)
        case 0xC000: case 0xC001: case 0xC002: case 0xC003:
            m_prgBank8k = value & 0x1F;
            updateBanks();
            break;
            
        // CHR Banks 0-3 (1KB each at $0000-$0FFF)
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
            
        // CHR Banks 4-7 (1KB each at $1000-$1FFF)
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
            
        // IRQ Latch
        case 0xF000:
            m_irqLatch = value;
            break;
            
        // IRQ Control
        case 0xF001:
            // D0: IRQ enable after acknowledge
            // D1: IRQ enable
            // D2: IRQ mode (0 = scanline, 1 = cycle)
            m_irqEnableOnAck = (value & 0x01) != 0;
            m_irqEnable = (value & 0x02) != 0;
            m_irqCycleMode = (value & 0x04) != 0;
            if (m_irqEnable) {
                m_irqCounter = m_irqLatch;
                m_irqPrescaler = 341;  // Reset prescaler
            }
            m_irqActive = false;  // Acknowledge pending IRQ
            break;
            
        // IRQ Acknowledge
        case 0xF002:
            m_irqEnable = m_irqEnableOnAck;
            m_irqActive = false;
            break;
    }
}

void Mapper024::clockIRQ() {
    if (m_irqCounter == 0xFF) {
        m_irqCounter = m_irqLatch;
        m_irqActive = true;
    } else {
        m_irqCounter++;
    }
}

u8 Mapper024::readCHR(u16 address) {
    u8 bank = address / 0x400;
    u16 offset = address & 0x3FF;
    return m_cartridge->getCHR()[m_chrBankOffset[bank] + offset];
}

void Mapper024::writeCHR(u16 address, u8 value) {
    // CHR RAM support
    u8 bank = address / 0x400;
    u16 offset = address & 0x3FF;
    m_cartridge->getCHR()[m_chrBankOffset[bank] + offset] = value;
}

MirrorMode Mapper024::getMirrorMode() const {
    return m_mirrorMode;
}

void Mapper024::scanlineCounter() {
    // VRC6 doesn't use scanline-based IRQ in the traditional sense
    // The IRQ is clocked every CPU cycle in clockAudio() instead
    // This method is kept for compatibility but does nothing for VRC6
}

void Mapper024::clockAudio() {
    // Clock expansion audio every CPU cycle
    m_audio.clock();
    
    // Clock VRC IRQ every CPU cycle
    if (m_irqEnable) {
        // Prescaler is decremented by 3 each CPU cycle (tracks PPU cycles)
        m_irqPrescaler -= 3;
        
        // In cycle mode, clock IRQ every CPU cycle
        // In scanline mode, clock IRQ when prescaler reaches 0 (~every 113 CPU cycles)
        if (m_irqCycleMode || (!m_irqCycleMode && m_irqPrescaler <= 0)) {
            if (m_irqCounter == 0xFF) {
                m_irqCounter = m_irqLatch;
                m_irqActive = true;
            } else {
                m_irqCounter++;
            }
            
            // Reset prescaler (341 PPU cycles per scanline)
            m_irqPrescaler += 341;
        }
    }
}

float Mapper024::getAudioOutput() const {
    return m_audio.getOutput();
}

void Mapper024::saveState(std::ofstream& file) const {
    file.write(reinterpret_cast<const char*>(&m_prgBank16k), sizeof(m_prgBank16k));
    file.write(reinterpret_cast<const char*>(&m_prgBank8k), sizeof(m_prgBank8k));
    file.write(reinterpret_cast<const char*>(m_chrBank), sizeof(m_chrBank));
    file.write(reinterpret_cast<const char*>(&m_bankingMode), sizeof(m_bankingMode));
    file.write(reinterpret_cast<const char*>(&m_mirrorMode), sizeof(m_mirrorMode));
    file.write(reinterpret_cast<const char*>(&m_irqLatch), sizeof(m_irqLatch));
    file.write(reinterpret_cast<const char*>(&m_irqCounter), sizeof(m_irqCounter));
    file.write(reinterpret_cast<const char*>(&m_irqEnable), sizeof(m_irqEnable));
    file.write(reinterpret_cast<const char*>(&m_irqEnableOnAck), sizeof(m_irqEnableOnAck));
    file.write(reinterpret_cast<const char*>(&m_irqCycleMode), sizeof(m_irqCycleMode));
    file.write(reinterpret_cast<const char*>(&m_irqPrescaler), sizeof(m_irqPrescaler));
    m_audio.saveState(file);
}

void Mapper024::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(&m_prgBank16k), sizeof(m_prgBank16k));
    file.read(reinterpret_cast<char*>(&m_prgBank8k), sizeof(m_prgBank8k));
    file.read(reinterpret_cast<char*>(m_chrBank), sizeof(m_chrBank));
    file.read(reinterpret_cast<char*>(&m_bankingMode), sizeof(m_bankingMode));
    file.read(reinterpret_cast<char*>(&m_mirrorMode), sizeof(m_mirrorMode));
    file.read(reinterpret_cast<char*>(&m_irqLatch), sizeof(m_irqLatch));
    file.read(reinterpret_cast<char*>(&m_irqCounter), sizeof(m_irqCounter));
    file.read(reinterpret_cast<char*>(&m_irqEnable), sizeof(m_irqEnable));
    file.read(reinterpret_cast<char*>(&m_irqEnableOnAck), sizeof(m_irqEnableOnAck));
    file.read(reinterpret_cast<char*>(&m_irqCycleMode), sizeof(m_irqCycleMode));
    file.read(reinterpret_cast<char*>(&m_irqPrescaler), sizeof(m_irqPrescaler));
    m_audio.loadState(file);
    updateBanks();
}

} // namespace nes

