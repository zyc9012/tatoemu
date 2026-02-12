#include "mapper005.h"
#include "../consts.h"
#include "../ppu.h"
#include <cstring>
namespace nes {

// ============================================================
// MMC5 Square Channel Implementation
// ============================================================

void MMC5Square::reset() {
    m_channel.reset();
    // Ensure sweep is disabled (MMC5 has no sweep)
    m_channel.sweep.reset();
    m_channel.sweep.enabled = false;
}

void MMC5Square::writeRegister(u8 addr, u8 value) {
    switch (addr & 0x03) {
        case 0:  // $5000/$5004: Control
            m_channel.writeControl(value);
            break;
            
        case 1:  // $5001/$5005: Sweep (no effect on MMC5, but we can ignore it)
            // MMC5 square channels don't have sweep, so this register has no effect
            // We don't call writeSweep() to keep sweep disabled
            break;
            
        case 2:  // $5002/$5006: Timer Low
            m_channel.writeTimerLow(value);
            break;
            
        case 3:  // $5003/$5007: Timer High + Length Counter
            m_channel.writeTimerHigh(value);
            break;
    }
}

u8 MMC5Square::getOutput() const {
    // Use PulseChannel's output, but we need to bypass sweep muting
    // since MMC5 doesn't mute at low frequencies
    if (m_channel.lengthCounter.isZero()) return 0;
    if (APU::PulseChannel::DUTY_TABLE[m_channel.dutyMode][m_channel.sequencerStep] == 0) return 0;
    
    return m_channel.envelope.volume();
}

void MMC5Square::saveState(Buffer* buf) {
    // Save the entire PulseChannel state
    buffer_write(buf, &m_channel.timerPeriod, sizeof(m_channel.timerPeriod));
    buffer_write(buf, &m_channel.timerCounter, sizeof(m_channel.timerCounter));
    buffer_write(buf, &m_channel.dutyMode, sizeof(m_channel.dutyMode));
    buffer_write(buf, &m_channel.sequencerStep, sizeof(m_channel.sequencerStep));
    buffer_write(buf, &m_channel.envelope, sizeof(m_channel.envelope));
    buffer_write(buf, &m_channel.sweep, sizeof(m_channel.sweep));
    buffer_write(buf, &m_channel.lengthCounter, sizeof(m_channel.lengthCounter));
    buffer_write(buf, &m_channel.isPulse1, sizeof(m_channel.isPulse1));
}

void MMC5Square::loadState(Buffer* buf) {
    // Load the entire PulseChannel state
    buffer_read(buf, &m_channel.timerPeriod, sizeof(m_channel.timerPeriod));
    buffer_read(buf, &m_channel.timerCounter, sizeof(m_channel.timerCounter));
    buffer_read(buf, &m_channel.dutyMode, sizeof(m_channel.dutyMode));
    buffer_read(buf, &m_channel.sequencerStep, sizeof(m_channel.sequencerStep));
    buffer_read(buf, &m_channel.envelope, sizeof(m_channel.envelope));
    buffer_read(buf, &m_channel.sweep, sizeof(m_channel.sweep));
    buffer_read(buf, &m_channel.lengthCounter, sizeof(m_channel.lengthCounter));
    buffer_read(buf, &m_channel.isPulse1, sizeof(m_channel.isPulse1));
    // Ensure sweep stays disabled
    m_channel.sweep.enabled = false;
}

// ============================================================
// MMC5 Audio Controller Implementation
// ============================================================

void MMC5Audio::reset() {
    m_square1.reset();
    m_square2.reset();
    m_pcmReadMode = false;
    m_pcmIrqEnabled = false;
    m_pcmOutput = 0;
    m_frameCounter = FRAME_COUNTER_PERIOD;  // Initialize to period so first clock happens after one period
    m_oddCycle = false;
}

void MMC5Audio::writeRegister(u16 addr, u8 value) {
    switch (addr) {
        case 0x5000: case 0x5001: case 0x5002: case 0x5003:
            m_square1.writeRegister(addr & 0x03, value);
            break;
            
        case 0x5004: case 0x5005: case 0x5006: case 0x5007:
            m_square2.writeRegister(addr & 0x03, value);
            break;
            
        case 0x5010:
            // PCM control
            // D0: PCM read mode
            // D7: PCM IRQ enable
            m_pcmReadMode = (value & 0x01) != 0;
            m_pcmIrqEnabled = (value & 0x80) != 0;
            // TODO: Implement PCM IRQ
            break;
            
        case 0x5011:
            // PCM output (only when not in read mode)
            if (!m_pcmReadMode && value != 0) {
                m_pcmOutput = value;
            }
            // TODO: Implement PCM read mode
            break;
            
        case 0x5015:
            // Channel enable (maps to $4015 behavior)
            m_square1.m_channel.lengthCounter.enabled = (value & 0x01) != 0;
            m_square2.m_channel.lengthCounter.enabled = (value & 0x02) != 0;
            
            // Disable channels: clear length counter
            if (!m_square1.m_channel.lengthCounter.enabled) {
                m_square1.m_channel.lengthCounter.counter = 0;
            }
            if (!m_square2.m_channel.lengthCounter.enabled) {
                m_square2.m_channel.lengthCounter.counter = 0;
            }
            break;
    }
}

u8 MMC5Audio::readRegister(u16 addr) {
    switch (addr) {
        case 0x5010:
            // PCM IRQ status (TODO: implement PCM IRQ)
            return 0;
            
        case 0x5015:
            // Channel status
            u8 status = 0;
            if (m_square1.isEnabled()) status |= 0x01;
            if (m_square2.isEnabled()) status |= 0x02;
            return status;
    }
    
    return 0;  // Open bus (simplified)
}

void MMC5Audio::clock() {
    // Toggle cycle counter
    m_oddCycle = !m_oddCycle;

    // Clock square channels every other CPU cycle (like APU pulse channels)
    if (m_oddCycle) {
        m_square1.clock();
        m_square2.clock();
    }

    // Clock envelope/length counter at ~240 Hz
    m_frameCounter--;
    if (m_frameCounter <= 0) {
        m_frameCounter = FRAME_COUNTER_PERIOD;
        m_square1.clockLengthCounter();
        m_square1.clockEnvelope();
        m_square2.clockLengthCounter();
        m_square2.clockEnvelope();
    }
}

float MMC5Audio::getOutput() const {
    // Get square channel outputs (0-15 each)
    u8 square1Out = m_square1.getOutput();
    u8 square2Out = m_square2.getOutput();
    u8 pcmOut = m_pcmOutput;
    
    // Combine outputs
    // Square channels output 0-15 each (like APU pulse channels)
    // PCM outputs 0-255, but is rarely used
    // For mixing, we treat PCM as equivalent to square channels for simplicity
    u32 squareTotal = square1Out + square2Out;
    u32 totalOutput = squareTotal;
    
    // Add PCM contribution (scale PCM to match square channel levels)
    // PCM is 8-bit, square channels are 4-bit, so scale PCM by 1/16
    if (pcmOut > 0) {
        totalOutput += (pcmOut >> 4);  // Scale PCM to 0-15 range
    }
    
    // Normalize to roughly match APU levels
    // MMC5 square channels have equivalent volume to APU pulse channels
    // Max output: 15 + 15 + 15 = 45 (if PCM is also at max scaled)
    // Normalize similar to VRC6 (max 61, normalized to 0.5)
    // For square-only: max 30, normalize to ~0.25 (half of VRC6 max)
    float normalized = static_cast<float>(totalOutput) / 60.0f * 0.5f;
    
    // Note: MMC5 polarity is reversed compared to APU according to reference,
    // but for simplicity we normalize as positive since the APU mixing
    // just adds expansion audio directly.
    return normalized;
}

void MMC5Audio::saveState(Buffer* buf) {
    m_square1.saveState(buf);
    m_square2.saveState(buf);
    buffer_write(buf, &m_pcmReadMode, sizeof(m_pcmReadMode));
    buffer_write(buf, &m_pcmIrqEnabled, sizeof(m_pcmIrqEnabled));
    buffer_write(buf, &m_pcmOutput, sizeof(m_pcmOutput));
    buffer_write(buf, &m_frameCounter, sizeof(m_frameCounter));
    buffer_write(buf, &m_oddCycle, sizeof(m_oddCycle));
}

void MMC5Audio::loadState(Buffer* buf) {
    m_square1.loadState(buf);
    m_square2.loadState(buf);
    buffer_read(buf, &m_pcmReadMode, sizeof(m_pcmReadMode));
    buffer_read(buf, &m_pcmIrqEnabled, sizeof(m_pcmIrqEnabled));
    buffer_read(buf, &m_pcmOutput, sizeof(m_pcmOutput));
    buffer_read(buf, &m_frameCounter, sizeof(m_frameCounter));
    buffer_read(buf, &m_oddCycle, sizeof(m_oddCycle));
}

// ============================================================
// Mapper005 Implementation
// ============================================================

Mapper005::Mapper005(Cartridge* cartridge)
    : Mapper(cartridge) {
}

void Mapper005::reset() {
    Mapper::reset();
    m_prgMode = 3;
    m_prgRamProtect1 = false;
    m_prgRamProtect2 = false;
    m_chrMode = 0;
    m_chrUpperBits = 0;
    m_lastChrReg = 0;
    m_nametableMapping = 0;
    m_fillModeTile = 0;
    m_fillModeAttr = 0;
    m_exRamMode = 0;
    m_irqScanline = 0;
    m_irqStatus = 0;
    m_irqEnable = false;
    m_inFrame = false;
    m_scanlineCounter = 0;
    m_multiplicand = 0;
    m_multiplier = 0;
    
    m_capturedExRam = 0;
    m_ppuFetchState = 0;
    m_splitMode = 0;
    m_splitScroll = 0;
    m_splitBank = 0;
    m_lastScanline = 0;
    
    std::memset(m_prgBankRegs, 0xFF, sizeof(m_prgBankRegs));
    std::memset(m_chrBankRegs, 0, sizeof(m_chrBankRegs));
    std::memset(m_prgBankOffset, 0, sizeof(m_prgBankOffset));
    std::memset(m_chrBankOffset, 0, sizeof(m_chrBankOffset));
    std::memset(m_chrBgBankOffset, 0, sizeof(m_chrBgBankOffset));
    m_exRam.fill(0);
    m_prgRamExt.fill(0);
    
    m_audio.reset();
    
    updatePRGBanks();
    updateCHRBanks();
}

void Mapper005::updatePRGBanks() {
    const auto& prg = m_cartridge->getPRG();
    u32 prgSize = prg.size();
    u32 prgBanks8k = prgSize / 0x2000;
    
    switch (m_prgMode) {
        case 0:  // 32KB mode
            {
                u8 bank = (m_prgBankRegs[4] & 0x7C) >> 2;
                u32 offset = (bank % (prgBanks8k / 4)) * 0x8000;
                m_prgBankOffset[0] = offset;
                m_prgBankOffset[1] = offset + 0x2000;
                m_prgBankOffset[2] = offset + 0x4000;
                m_prgBankOffset[3] = offset + 0x6000;
            }
            break;
            
        case 1:  // 16KB + 16KB mode
            {
                u8 bank0 = (m_prgBankRegs[2] & 0x7E) >> 1;
                u8 bank1 = (m_prgBankRegs[4] & 0x7E) >> 1;
                m_prgBankOffset[0] = ((bank0 % (prgBanks8k / 2)) * 0x4000);
                m_prgBankOffset[1] = m_prgBankOffset[0] + 0x2000;
                m_prgBankOffset[2] = ((bank1 % (prgBanks8k / 2)) * 0x4000);
                m_prgBankOffset[3] = m_prgBankOffset[2] + 0x2000;
            }
            break;
            
        case 2:  // 16KB + 8KB + 8KB mode
            {
                u8 bank0 = (m_prgBankRegs[2] & 0x7E) >> 1;
                u8 bank1 = m_prgBankRegs[3] & 0x7F;
                u8 bank2 = m_prgBankRegs[4] & 0x7F;
                m_prgBankOffset[0] = ((bank0 % (prgBanks8k / 2)) * 0x4000);
                m_prgBankOffset[1] = m_prgBankOffset[0] + 0x2000;
                m_prgBankOffset[2] = (bank1 % prgBanks8k) * 0x2000;
                m_prgBankOffset[3] = (bank2 % prgBanks8k) * 0x2000;
            }
            break;
            
        case 3:  // 8KB x 4 mode
            {
                u8 bank0 = m_prgBankRegs[1] & 0x7F;
                u8 bank1 = m_prgBankRegs[2] & 0x7F;
                u8 bank2 = m_prgBankRegs[3] & 0x7F;
                u8 bank3 = m_prgBankRegs[4] & 0x7F;
                m_prgBankOffset[0] = (bank0 % prgBanks8k) * 0x2000;
                m_prgBankOffset[1] = (bank1 % prgBanks8k) * 0x2000;
                m_prgBankOffset[2] = (bank2 % prgBanks8k) * 0x2000;
                m_prgBankOffset[3] = (bank3 % prgBanks8k) * 0x2000;
            }
            break;
    }
}

void Mapper005::updateCHRBanks() {
    const auto& chr = m_cartridge->getCHR();
    u32 chrSize = chr.size();
    
    // Calculate offsets for sprite banks (registers 0-7)
    // and background banks (registers 8-11)
    switch (m_chrMode) {
        case 0:  // 8KB mode - 1 bank
            {
                // Sprite banks: use register 7
                u16 bank = m_chrBankRegs[7] & 0x3FF;  // 10-bit bank number
                u32 chrBanks8k = chrSize / 0x2000;
                u32 offset = (bank % chrBanks8k) * 0x2000;
                for (int i = 0; i < 8; i++) {
                    m_chrBankOffset[i] = offset + (i * 0x400);
                }
                
                // Background banks: use register 11
                bank = m_chrBankRegs[11] & 0x3FF;
                offset = (bank % chrBanks8k) * 0x2000;
                for (int i = 0; i < 8; i++) {
                    m_chrBgBankOffset[i] = offset + (i * 0x400);
                }
            }
            break;
            
        case 1:  // 4KB mode - 2 banks
            {
                u32 chrBanks4k = chrSize / 0x1000;
                
                // Sprite banks: use registers 3 and 7
                u16 bank0 = m_chrBankRegs[3] & 0x3FF;
                u16 bank1 = m_chrBankRegs[7] & 0x3FF;
                u32 offset0 = (bank0 % chrBanks4k) * 0x1000;
                u32 offset1 = (bank1 % chrBanks4k) * 0x1000;
                
                m_chrBankOffset[0] = offset0;
                m_chrBankOffset[1] = offset0 + 0x400;
                m_chrBankOffset[2] = offset0 + 0x800;
                m_chrBankOffset[3] = offset0 + 0xC00;
                m_chrBankOffset[4] = offset1;
                m_chrBankOffset[5] = offset1 + 0x400;
                m_chrBankOffset[6] = offset1 + 0x800;
                m_chrBankOffset[7] = offset1 + 0xC00;
                
                // Background banks: use registers 11 (both halves)
                bank0 = m_chrBankRegs[11] & 0x3FF;
                offset0 = (bank0 % chrBanks4k) * 0x1000;
                m_chrBgBankOffset[0] = offset0;
                m_chrBgBankOffset[1] = offset0 + 0x400;
                m_chrBgBankOffset[2] = offset0 + 0x800;
                m_chrBgBankOffset[3] = offset0 + 0xC00;
                m_chrBgBankOffset[4] = offset0;
                m_chrBgBankOffset[5] = offset0 + 0x400;
                m_chrBgBankOffset[6] = offset0 + 0x800;
                m_chrBgBankOffset[7] = offset0 + 0xC00;
            }
            break;
            
        case 2:  // 2KB mode - 4 banks
            {
                u32 chrBanks2k = chrSize / 0x800;
                
                // Sprite banks: use registers 1, 3, 5, 7
                u16 bank0 = m_chrBankRegs[1] & 0x3FF;
                u16 bank1 = m_chrBankRegs[3] & 0x3FF;
                u16 bank2 = m_chrBankRegs[5] & 0x3FF;
                u16 bank3 = m_chrBankRegs[7] & 0x3FF;
                
                m_chrBankOffset[0] = ((bank0 % chrBanks2k) * 0x800);
                m_chrBankOffset[1] = m_chrBankOffset[0] + 0x400;
                m_chrBankOffset[2] = ((bank1 % chrBanks2k) * 0x800);
                m_chrBankOffset[3] = m_chrBankOffset[2] + 0x400;
                m_chrBankOffset[4] = ((bank2 % chrBanks2k) * 0x800);
                m_chrBankOffset[5] = m_chrBankOffset[4] + 0x400;
                m_chrBankOffset[6] = ((bank3 % chrBanks2k) * 0x800);
                m_chrBankOffset[7] = m_chrBankOffset[6] + 0x400;
                
                // Background banks: use registers 9, 11, 9, 11 (as per reference)
                bank0 = m_chrBankRegs[9] & 0x3FF;
                bank1 = m_chrBankRegs[11] & 0x3FF;
                
                m_chrBgBankOffset[0] = ((bank0 % chrBanks2k) * 0x800);
                m_chrBgBankOffset[1] = m_chrBgBankOffset[0] + 0x400;
                m_chrBgBankOffset[2] = ((bank1 % chrBanks2k) * 0x800);
                m_chrBgBankOffset[3] = m_chrBgBankOffset[2] + 0x400;
                m_chrBgBankOffset[4] = ((bank0 % chrBanks2k) * 0x800);
                m_chrBgBankOffset[5] = m_chrBgBankOffset[4] + 0x400;
                m_chrBgBankOffset[6] = ((bank1 % chrBanks2k) * 0x800);
                m_chrBgBankOffset[7] = m_chrBgBankOffset[6] + 0x400;
            }
            break;
            
        case 3:  // 1KB mode - 8 banks
            {
                u32 chrBanks1k = chrSize / 0x400;
                
                // Sprite banks: use registers 0-7
                for (int i = 0; i < 8; i++) {
                    u16 bank = m_chrBankRegs[i] & 0x3FF;
                    m_chrBankOffset[i] = (bank % chrBanks1k) * 0x400;
                }
                
                // Background banks: use registers 8, 9, 10, 11, 8, 9, 10, 11
                m_chrBgBankOffset[0] = ((m_chrBankRegs[8] & 0x3FF) % chrBanks1k) * 0x400;
                m_chrBgBankOffset[1] = ((m_chrBankRegs[9] & 0x3FF) % chrBanks1k) * 0x400;
                m_chrBgBankOffset[2] = ((m_chrBankRegs[10] & 0x3FF) % chrBanks1k) * 0x400;
                m_chrBgBankOffset[3] = ((m_chrBankRegs[11] & 0x3FF) % chrBanks1k) * 0x400;
                m_chrBgBankOffset[4] = ((m_chrBankRegs[8] & 0x3FF) % chrBanks1k) * 0x400;
                m_chrBgBankOffset[5] = ((m_chrBankRegs[9] & 0x3FF) % chrBanks1k) * 0x400;
                m_chrBgBankOffset[6] = ((m_chrBankRegs[10] & 0x3FF) % chrBanks1k) * 0x400;
                m_chrBgBankOffset[7] = ((m_chrBankRegs[11] & 0x3FF) % chrBanks1k) * 0x400;
            }
            break;
    }
}

s8 Mapper005::getPRGRamBank(u16 address) {
    // Determine which PRG bank register controls this address based on PRG mode
    u8 regIndex;
    bool isRam;
    
    if (m_prgMode == 0) {
        // Mode 0: Register 4 ($5117) controls $8000-$FFFF (always ROM)
        return -1;  // ROM
    } else if (m_prgMode == 1) {
        // Mode 1: Register 2 ($5115) controls $8000-$BFFF, Register 4 ($5117) controls $C000-$FFFF
        if (address < 0xC000) {
            regIndex = 2;
            isRam = (m_prgBankRegs[2] & 0x80) == 0;
        } else {
            return -1;  // Register 4 always uses ROM
        }
    } else if (m_prgMode == 2) {
        // Mode 2: Register 2 ($5115) controls $8000-$BFFF, Register 3 ($5116) controls $C000-$DFFF, Register 4 ($5117) controls $E000-$FFFF
        if (address < 0xC000) {
            regIndex = 2;
            isRam = (m_prgBankRegs[2] & 0x80) == 0;
        } else if (address < 0xE000) {
            regIndex = 3;
            isRam = (m_prgBankRegs[3] & 0x80) == 0;
        } else {
            return -1;  // Register 4 always uses ROM
        }
    } else {  // m_prgMode == 3
        // Mode 3: Register 1 ($5114) controls $8000-$9FFF, Register 2 ($5115) controls $A000-$BFFF, 
        //         Register 3 ($5116) controls $C000-$DFFF, Register 4 ($5117) controls $E000-$FFFF
        if (address < 0xA000) {
            regIndex = 1;
            isRam = (m_prgBankRegs[1] & 0x80) == 0;
        } else if (address < 0xC000) {
            regIndex = 2;
            isRam = (m_prgBankRegs[2] & 0x80) == 0;
        } else if (address < 0xE000) {
            regIndex = 3;
            isRam = (m_prgBankRegs[3] & 0x80) == 0;
        } else {
            return -1;  // Register 4 always uses ROM
        }
    }
    
    // Return RAM bank if it's RAM, otherwise -1 for ROM
    if (isRam) {
        return m_prgBankRegs[regIndex] & 0x07;  // Lower 3 bits select bank (0-7)
    } else {
        return -1;  // ROM
    }
}

u8 Mapper005::cpuRead(u16 address) {
    if (address >= 0x5000 && address < 0x5C00) {
        // MMC5 registers
        switch (address) {
            case 0x5010:  // PCM control/status
            case 0x5015:  // Audio status
                return m_audio.readRegister(address);
            case 0x5204:  // IRQ Status
                {
                    u8 result = m_irqStatus;
                    m_irqStatus &= ~0x80;  // Clear pending flag on read
                    m_cartridge->getCPU()->irq(0);
                    return result;
                }
            case 0x5205:  // Multiply result low
                return (m_multiplicand * m_multiplier) & 0xFF;
            case 0x5206:  // Multiply result high
                return ((m_multiplicand * m_multiplier) >> 8) & 0xFF;
            default:
                return 0;
        }
    } else if (address >= 0x5C00 && address < 0x6000) {
        // ExRAM
        return readExRAM(address - 0x5C00);
    } else if (address >= 0x6000 && address < 0x8000) {
        // PRG RAM
        u8 bank = m_prgBankRegs[0] & 0x07;  // Lower 3 bits select bank (0-7)
        u16 offset = address & 0x1FFF;  // Offset within 8KB bank
        return m_prgRamExt[(bank * 0x2000) + offset];
    } else if (address >= 0x8000) {
        // PRG ROM/RAM - Determine if this address maps to RAM or ROM
        s8 ramBank = getPRGRamBank(address);
        
        if (ramBank >= 0) {
            // PRG RAM
            u16 offset = address & 0x1FFF;  // Offset within 8KB bank
            return m_prgRamExt[(ramBank * 0x2000) + offset];
        } else {
            // PRG ROM
            u8 bank = (address - 0x8000) / 0x2000;
            u16 offset = address & 0x1FFF;
            
            const auto& prg = m_cartridge->getPRG();
            u32 prgOffset = m_prgBankOffset[bank] + offset;
            prgOffset = prgOffset % prg.size();
            return prg[prgOffset];
        }
    }
    return 0;
}

void Mapper005::cpuWrite(u16 address, u8 value) {
    if (address >= 0x5000 && address < 0x5C00) {
        // MMC5 audio registers ($5000-$5015)
        if (address >= 0x5000 && address <= 0x5015) {
            m_audio.writeRegister(address, value);
            return;
        }
        
        // MMC5 other registers
        switch (address) {
            case 0x5100:  // PRG mode
                m_prgMode = value & 0x03;
                updatePRGBanks();
                break;
            case 0x5101:  // CHR mode
                m_chrMode = value & 0x03;
                updateCHRBanks();
                break;
            case 0x5102:  // PRG RAM protect 1
                m_prgRamProtect1 = (value & 0x03) == 0x02;
                break;
            case 0x5103:  // PRG RAM protect 2
                m_prgRamProtect2 = (value & 0x03) == 0x01;
                break;
            case 0x5104:  // Extended RAM mode
                m_exRamMode = value & 0x03;
                break;
            case 0x5105:  // Nametable mapping
                m_nametableMapping = value;
                break;
            case 0x5106:  // Fill mode tile
                m_fillModeTile = value;
                break;
            case 0x5107:  // Fill mode attribute
                m_fillModeAttr = value & 0x03;
                break;
            case 0x5113:  // PRG bank 0 (RAM)
                m_prgBankRegs[0] = value;
                updatePRGBanks();
                break;
            case 0x5114:  // PRG bank 1
                m_prgBankRegs[1] = value;
                updatePRGBanks();
                break;
            case 0x5115:  // PRG bank 2
                m_prgBankRegs[2] = value;
                updatePRGBanks();
                break;
            case 0x5116:  // PRG bank 3
                m_prgBankRegs[3] = value;
                updatePRGBanks();
                break;
            case 0x5117:  // PRG bank 4
                m_prgBankRegs[4] = value | 0x80;  // Always ROM
                updatePRGBanks();
                break;
            case 0x5120: case 0x5121: case 0x5122: case 0x5123:
            case 0x5124: case 0x5125: case 0x5126: case 0x5127:
                // CHR banks (sprite) - combine with upper bits from $5130
                {
                    u16 newValue = value | (m_chrUpperBits << 8);
                    u8 regIndex = address - 0x5120;
                    if (newValue != m_chrBankRegs[regIndex] || m_lastChrReg != address) {
                        m_chrBankRegs[regIndex] = newValue;
                        m_lastChrReg = address;
                        updateCHRBanks();
                    }
                }
                break;
            case 0x5128: case 0x5129: case 0x512A: case 0x512B:
                // CHR banks (background) - combine with upper bits from $5130
                {
                    u16 newValue = value | (m_chrUpperBits << 8);
                    u8 regIndex = 8 + (address - 0x5128);
                    if (newValue != m_chrBankRegs[regIndex] || m_lastChrReg != address) {
                        m_chrBankRegs[regIndex] = newValue;
                        m_lastChrReg = address;
                        updateCHRBanks();
                    }
                }
                break;
            case 0x5130:  // CHR upper bits (bits 8-9 of 10-bit CHR banks)
                m_chrUpperBits = value & 0x03;
                // Update all CHR banks with new upper bits
                for (int i = 0; i < 12; i++) {
                    m_chrBankRegs[i] = (m_chrBankRegs[i] & 0xFF) | (m_chrUpperBits << 8);
                }
                updateCHRBanks();
                break;
            case 0x5200: // Split Mode
                m_splitMode = value;
                break;
            case 0x5201: // Split Scroll
                m_splitScroll = value;
                break;
            case 0x5202: // Split Bank
                m_splitBank = value;
                break;
            case 0x5203:  // IRQ scanline
                m_irqScanline = value;
                m_cartridge->getCPU()->irq(0);
                break;
            case 0x5204:  // IRQ enable
                m_irqEnable = (value & 0x80) != 0;
                m_cartridge->getCPU()->irq(0);
                break;
            case 0x5205:  // Multiplicand
                m_multiplicand = value;
                break;
            case 0x5206:  // Multiplier
                m_multiplier = value;
                break;
        }
    } else if (address >= 0x5C00 && address < 0x6000) {
        // ExRAM
        writeExRAM(address - 0x5C00, value);
    } else if (address >= 0x6000 && address < 0x8000) {
        // PRG RAM (if write-enabled)
        if (m_prgRamProtect1 && m_prgRamProtect2) {
            u8 bank = m_prgBankRegs[0] & 0x07;  // Lower 3 bits select bank (0-7)
            u16 offset = address & 0x1FFF;  // Offset within 8KB bank
            m_prgRamExt[(bank * 0x2000) + offset] = value;
        }
    } else if (address >= 0x8000) {
        // PRG RAM/ROM writes - Check if this address maps to RAM
        s8 ramBank = getPRGRamBank(address);
        
        // Write to PRG RAM if mapped and write-enabled
        if (ramBank >= 0 && m_prgRamProtect1 && m_prgRamProtect2) {
            u16 offset = address & 0x1FFF;
            m_prgRamExt[(ramBank * 0x2000) + offset] = value;
        }
        // ROM writes are ignored
    }
}

u8 Mapper005::readExRAM(u16 address) {
    if (m_exRamMode >= 2) {
        return m_exRam[address & 0x3FF];
    }
    return 0;  // Mode 0-1 returns open bus
}

void Mapper005::writeExRAM(u16 address, u8 value) {
    if (m_exRamMode != 3) {  // Mode 3 is read-only
        m_exRam[address & 0x3FF] = value;
    }
}

u32 Mapper005::mapCHR(u16 address) {
    // Determine if this is a background fetch from PPU state.
    PPU* ppu = m_cartridge->getPPU();
    bool isBgFetch = ppu->isFetchingBackgroundPattern();
    
    // Handle ExRAM Mode 1 banking (uses captured ExRAM byte for bank selection)
    if (isBgFetch && m_exRamMode == 1) {
        // MMC5 ExRAM Mode 1 (Extended Attributes):
        //   7..6 = palette select (AA)
        //   5..0 = 4KB CHR bank select (CCCCCC)
        // Pattern data for the current background tile comes from the 4KB bank
        // selected by bits 0-5 of the *captured* ExRAM byte.
        // Upper bits from $5130 are also used: bits 6-7 of bank come from $5130
        u32 bankIndex = (m_capturedExRam & 0x3F) | (m_chrUpperBits << 6);
        
        return bankIndex * 0x1000 + (address & 0x0FFF);
    }

    const u32* bankOffset;

    if (ppu->getSpriteHeight() == 16) {
        if (isBgFetch) {
            bankOffset = m_chrBgBankOffset;
        } else if (ppu->isFetchingSpritePattern()) {
            bankOffset = m_chrBankOffset;
        } else {
            bankOffset = (m_lastChrReg >= 0x5120 && m_lastChrReg <= 0x5127) ? m_chrBankOffset : m_chrBgBankOffset;
        }
    } else {
        bankOffset = m_chrBankOffset;
    }

    u8 bank = address / 0x400;
    u16 offset = address & 0x3FF;
    return bankOffset[bank] + offset;
}

u8 Mapper005::readCHR(u16 address) {
    return m_cartridge->getCHR()[mapCHR(address)];
}

void Mapper005::writeCHR(u16 address, u8 value) {
    m_cartridge->getCHR()[mapCHR(address)] = value;
}

bool Mapper005::readNametable(u16 address, u8& value) {
    bool isAttribute = ((address & 0x03C0) == 0x03C0);
    
    u8 nt = (address >> 10) & 0x03;
    u8 mode = (m_nametableMapping >> (nt * 2)) & 0x03;

    if (isAttribute) {
        m_ppuFetchState = 2; // Next is Pattern Low
        
        if (m_exRamMode == 1) {
            // ExRAM Mode 1: Use ExRAM for attributes
            // MMC5 provides per-tile palette in bits 6-7 of captured ExRAM.
            u8 pal = (m_capturedExRam >> 6) & 0x03;
            // Replicate 2 bits to full byte: 00 00 00 00 -> P P P P
            value = (pal << 6) | (pal << 4) | (pal << 2) | pal;
            return true;
        } else if (mode == 3) {
            // Fill Mode: Use fill mode attribute
            u8 pal = m_fillModeAttr & 0x03;
            value = (pal << 6) | (pal << 4) | (pal << 2) | pal;
            return true;
        }
        
        if (mode == 0) { // CIRAM 0
             value = m_cartridge->readCIRAM(address & 0x03FF);
             return true;
        } else if (mode == 1) { // CIRAM 1
             value = m_cartridge->readCIRAM(0x400 | (address & 0x03FF));
             return true;
        } else if (mode == 2) { // ExRAM as NT
             value = m_exRam[address & 0x03FF];
             return true;
        }
        
        return false;
    } 
    else {
        // Nametable Fetch
        m_ppuFetchState = 1; // Next is Attribute
        
        // Capture ExRAM for next steps
        m_capturedExRam = m_exRam[address & 0x03FF];
        
        switch (mode) {
            case 0: // CIRAM 0
                value = m_cartridge->readCIRAM(address & 0x03FF);
                return true;
            case 1: // CIRAM 1
                value = m_cartridge->readCIRAM(0x400 | (address & 0x03FF));
                return true;
            case 2: // ExRAM as NT
                value = m_exRam[address & 0x03FF];
                return true;
            case 3: // Fill Mode
                value = m_fillModeTile;
                return true;
        }
    }
    
    return false;
}

bool Mapper005::writeNametable(u16 address, u8 value) {
    // MMC5 nametable mapping via $5105 is per-nametable (not simple mirroring),
    // so PPU writes must respect it as well.
    address &= 0x3FFF;
    if (address < 0x2000 || address >= 0x3F00) return false;

    u8 nt = (address >> 10) & 0x03;
    u8 mode = (m_nametableMapping >> (nt * 2)) & 0x03;

    switch (mode) {
        case 0: // CIRAM page 0
            m_cartridge->writeCIRAM(address & 0x03FF, value);
            return true;
        case 1: // CIRAM page 1
            m_cartridge->writeCIRAM(0x400 | (address & 0x03FF), value);
            return true;
        case 2: // ExRAM (1KB)
            m_exRam[address & 0x03FF] = value;
            return true;
        case 3: // Fill mode - no backing storage (writes effectively ignored)
        default:
            return true;
    }
}

void Mapper005::scanlineCounter() {
    // Get current scanline from PPU
    PPU* ppu = m_cartridge->getPPU();
    if (!ppu) return;
    
    u16 currentScanline = ppu->getScanline();
    
    // Detect scanline change
    if (currentScanline != m_lastScanline) {
        // Check if we're entering a new frame (scanline 0, or wrapping from 261)
        if (currentScanline == 0) {
            // Entering new frame
            m_inFrame = true;
            m_irqStatus |= 0x40;
            m_scanlineCounter = 0;
            m_ppuFetchState = 0;  // Reset fetch state for new frame
            m_cartridge->getCPU()->irq(0);
        } else if (m_inFrame) {
            // Within frame - update scanline counter
            // Only count visible scanlines (0-239)
            if (currentScanline < 240) {
                m_scanlineCounter = currentScanline;
                
                // Check for IRQ
                if (m_scanlineCounter == m_irqScanline) {
                    m_irqStatus |= 0x80;
                    if (m_irqEnable) {
                        m_cartridge->getCPU()->irq(1);
                    }
                }
            } else if (currentScanline >= 240) {
                // End of visible frame
                m_inFrame = false;
                m_irqStatus &= ~0x40;
                m_scanlineCounter = 0; // Reset for next frame
            }
        }
        
        m_lastScanline = currentScanline;
    }
}

void Mapper005::clockAudio() {
    m_audio.clock();
}

float Mapper005::getAudioOutput() const {
    return m_audio.getOutput();
}

void Mapper005::saveState(Buffer* buf) {
    Mapper::saveState(buf);
    buffer_write(buf, &m_prgMode, sizeof(m_prgMode));
    buffer_write(buf, m_prgBankRegs, sizeof(m_prgBankRegs));
    buffer_write(buf, &m_prgRamProtect1, sizeof(m_prgRamProtect1));
    buffer_write(buf, &m_prgRamProtect2, sizeof(m_prgRamProtect2));
    buffer_write(buf, &m_chrMode, sizeof(m_chrMode));
    buffer_write(buf, &m_chrUpperBits, sizeof(m_chrUpperBits));
    buffer_write(buf, m_chrBankRegs, sizeof(m_chrBankRegs));
    buffer_write(buf, &m_lastChrReg, sizeof(m_lastChrReg));
    buffer_write(buf, &m_nametableMapping, sizeof(m_nametableMapping));
    buffer_write(buf, &m_fillModeTile, sizeof(m_fillModeTile));
    buffer_write(buf, &m_fillModeAttr, sizeof(m_fillModeAttr));
    buffer_write(buf, m_exRam.data(), m_exRam.size());
    buffer_write(buf, &m_exRamMode, sizeof(m_exRamMode));
    buffer_write(buf, &m_irqScanline, sizeof(m_irqScanline));
    buffer_write(buf, &m_irqStatus, sizeof(m_irqStatus));
    buffer_write(buf, &m_irqEnable, sizeof(m_irqEnable));
    buffer_write(buf, &m_inFrame, sizeof(m_inFrame));
    buffer_write(buf, &m_scanlineCounter, sizeof(m_scanlineCounter));
    buffer_write(buf, &m_multiplicand, sizeof(m_multiplicand));
    buffer_write(buf, &m_multiplier, sizeof(m_multiplier));
    buffer_write(buf, m_prgRamExt.data(), m_prgRamExt.size());
    buffer_write(buf, &m_capturedExRam, sizeof(m_capturedExRam));
    buffer_write(buf, &m_ppuFetchState, sizeof(m_ppuFetchState));
    buffer_write(buf, &m_splitMode, sizeof(m_splitMode));
    buffer_write(buf, &m_splitScroll, sizeof(m_splitScroll));
    buffer_write(buf, &m_splitBank, sizeof(m_splitBank));
    buffer_write(buf, &m_lastScanline, sizeof(m_lastScanline));
    
    // Save audio state
    m_audio.saveState(buf);
}

void Mapper005::loadState(Buffer* buf) {
    Mapper::loadState(buf);
    buffer_read(buf, &m_prgMode, sizeof(m_prgMode));
    buffer_read(buf, m_prgBankRegs, sizeof(m_prgBankRegs));
    buffer_read(buf, &m_prgRamProtect1, sizeof(m_prgRamProtect1));
    buffer_read(buf, &m_prgRamProtect2, sizeof(m_prgRamProtect2));
    buffer_read(buf, &m_chrMode, sizeof(m_chrMode));
    buffer_read(buf, &m_chrUpperBits, sizeof(m_chrUpperBits));
    buffer_read(buf, m_chrBankRegs, sizeof(m_chrBankRegs));
    buffer_read(buf, &m_lastChrReg, sizeof(m_lastChrReg));
    buffer_read(buf, &m_nametableMapping, sizeof(m_nametableMapping));
    buffer_read(buf, &m_fillModeTile, sizeof(m_fillModeTile));
    buffer_read(buf, &m_fillModeAttr, sizeof(m_fillModeAttr));
    buffer_read(buf, m_exRam.data(), m_exRam.size());
    buffer_read(buf, &m_exRamMode, sizeof(m_exRamMode));
    buffer_read(buf, &m_irqScanline, sizeof(m_irqScanline));
    buffer_read(buf, &m_irqStatus, sizeof(m_irqStatus));
    buffer_read(buf, &m_irqEnable, sizeof(m_irqEnable));
    buffer_read(buf, &m_inFrame, sizeof(m_inFrame));
    buffer_read(buf, &m_scanlineCounter, sizeof(m_scanlineCounter));
    buffer_read(buf, &m_multiplicand, sizeof(m_multiplicand));
    buffer_read(buf, &m_multiplier, sizeof(m_multiplier));
    buffer_read(buf, m_prgRamExt.data(), m_prgRamExt.size());
    buffer_read(buf, &m_capturedExRam, sizeof(m_capturedExRam));
    buffer_read(buf, &m_ppuFetchState, sizeof(m_ppuFetchState));
    buffer_read(buf, &m_splitMode, sizeof(m_splitMode));
    buffer_read(buf, &m_splitScroll, sizeof(m_splitScroll));
    buffer_read(buf, &m_splitBank, sizeof(m_splitBank));
    buffer_read(buf, &m_lastScanline, sizeof(m_lastScanline));
    
    // Load audio state
    m_audio.loadState(buf);
    
    updatePRGBanks();
    updateCHRBanks();
}

} // namespace nes
