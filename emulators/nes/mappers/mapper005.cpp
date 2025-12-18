#include "mapper005.h"
#include "../consts.h"
#include "../ppu.h"
#include <cstring>
namespace nes {

Mapper005::Mapper005(Cartridge* cartridge)
    : Mapper(cartridge)
    , m_prgMode(3)
    , m_prgRamProtect1(false)
    , m_prgRamProtect2(false)
    , m_chrMode(0)
    , m_chrUpperBits(0)
    , m_lastChrReg(0)
    , m_nametableMapping(0)
    , m_fillModeTile(0)
    , m_fillModeAttr(0)
    , m_exRamMode(0)
    , m_irqScanline(0)
    , m_irqStatus(0)
    , m_irqEnable(false)
    , m_inFrame(false)
    , m_scanlineCounter(0)
    , m_multiplicand(0)
    , m_multiplier(0)
    , m_capturedExRam(0)
    , m_ppuFetchState(0)
    , m_splitMode(0)
    , m_splitScroll(0)
    , m_splitBank(0)
    , m_lastScanline(0) {
    std::memset(m_prgBankRegs, 0xFF, sizeof(m_prgBankRegs));
    std::memset(m_chrBankRegs, 0, sizeof(m_chrBankRegs));
    std::memset(m_prgBankOffset, 0, sizeof(m_prgBankOffset));
    std::memset(m_chrBankOffset, 0, sizeof(m_chrBankOffset));
    std::memset(m_chrBgBankOffset, 0, sizeof(m_chrBgBankOffset));
    m_exRam.fill(0);
    m_prgRamExt.fill(0);
}

void Mapper005::reset() {
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
    m_irqActive = false;
    
    m_capturedExRam = 0;
    m_ppuFetchState = 0;
    m_splitMode = 0;
    m_splitScroll = 0;
    m_splitBank = 0;
    m_lastScanline = 0;
    
    std::memset(m_prgBankRegs, 0xFF, sizeof(m_prgBankRegs));
    std::memset(m_chrBankRegs, 0, sizeof(m_chrBankRegs));
    std::memset(m_chrBgBankOffset, 0, sizeof(m_chrBgBankOffset));
    m_exRam.fill(0);
    
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
    if (chrSize == 0) return;
    
    u32 chrBanks1k = chrSize / 0x400;
    if (chrBanks1k == 0) chrBanks1k = 1;
    
    // Update both sprite and BG CHR bank offsets.
    // readCHR() will choose which set to use based on:
    // - During active rendering: PPU fetch state determines sprite vs BG
    // - Outside active rendering: last written register set determines sprite vs BG
    switch (m_chrMode) {
        case 0:  // 8KB mode
            {
                u16 bank = m_chrBankRegs[7] & 0x3FF;  // Use full 10-bit bank
                u32 offset = (bank % (chrBanks1k / 8)) * 0x2000;
                for (int i = 0; i < 8; i++) {
                    m_chrBankOffset[i] = offset + i * 0x400;
                }
            }
            break;
            
        case 1:  // 4KB mode
            {
                u16 bank0 = m_chrBankRegs[3] & 0x3FF;
                u16 bank1 = m_chrBankRegs[7] & 0x3FF;
                u32 offset0 = (bank0 % (chrBanks1k / 4)) * 0x1000;
                u32 offset1 = (bank1 % (chrBanks1k / 4)) * 0x1000;
                for (int i = 0; i < 4; i++) {
                    m_chrBankOffset[i] = offset0 + i * 0x400;
                    m_chrBankOffset[i + 4] = offset1 + i * 0x400;
                }
            }
            break;
            
        case 2:  // 2KB mode
            {
                u16 bank0 = m_chrBankRegs[1] & 0x3FF;
                u16 bank1 = m_chrBankRegs[3] & 0x3FF;
                u16 bank2 = m_chrBankRegs[5] & 0x3FF;
                u16 bank3 = m_chrBankRegs[7] & 0x3FF;
                m_chrBankOffset[0] = ((bank0 % (chrBanks1k / 2)) * 0x800);
                m_chrBankOffset[1] = m_chrBankOffset[0] + 0x400;
                m_chrBankOffset[2] = ((bank1 % (chrBanks1k / 2)) * 0x800);
                m_chrBankOffset[3] = m_chrBankOffset[2] + 0x400;
                m_chrBankOffset[4] = ((bank2 % (chrBanks1k / 2)) * 0x800);
                m_chrBankOffset[5] = m_chrBankOffset[4] + 0x400;
                m_chrBankOffset[6] = ((bank3 % (chrBanks1k / 2)) * 0x800);
                m_chrBankOffset[7] = m_chrBankOffset[6] + 0x400;
            }
            break;
            
        case 3:  // 1KB mode
            for (int i = 0; i < 8; i++) {
                u16 bank = m_chrBankRegs[i] & 0x3FF;
                m_chrBankOffset[i] = (bank % chrBanks1k) * 0x400;
            }
            break;
    }

    // Background CHR banks come from the dedicated BG registers ($5128-$512B).
    // Each selects a 1KB slice; mirror to both halves so either BG pattern
    // table selection works without extra checks.
    for (int i = 0; i < 4; i++) {
        u16 bank = m_chrBankRegs[8 + i] & 0x3FF;  // Use full 10-bit bank
        u32 offset = (bank % chrBanks1k) * 0x400;
        m_chrBgBankOffset[i] = offset;       // Pattern table at $0000
        m_chrBgBankOffset[i + 4] = offset;   // Pattern table at $1000
    }
}

u8 Mapper005::cpuRead(u16 address) {
    if (address >= 0x5000 && address < 0x5C00) {
        // MMC5 registers
        switch (address) {
            case 0x5204:  // IRQ Status
                {
                    u8 result = m_irqStatus;
                    m_irqStatus &= ~0x80;  // Clear pending flag on read
                    m_irqActive = false;
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
        return m_prgRamExt[address & 0x1FFF];
    } else if (address >= 0x8000) {
        // PRG ROM
        u8 bank = (address - 0x8000) / 0x2000;
        u16 offset = address & 0x1FFF;
        
        // Check if bank points to RAM or ROM
        u8 bankReg = m_prgBankRegs[(m_prgMode == 3) ? (bank + 1) : ((bank < 2) ? 2 : 4)];
        if (!(bankReg & 0x80)) {
            // RAM bank
            return m_prgRamExt[(bankReg & 0x07) * 0x2000 + offset];
        }
        return m_cartridge->getPRG()[m_prgBankOffset[bank] + offset];
    }
    return 0;
}

void Mapper005::cpuWrite(u16 address, u8 value) {
    if (address >= 0x5000 && address < 0x5C00) {
        // MMC5 registers
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
                break;
            case 0x5204:  // IRQ enable
                m_irqEnable = (value & 0x80) != 0;
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
            m_prgRamExt[address & 0x1FFF] = value;
        }
    }
    // ROM writes ignored
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

u8 Mapper005::readCHR(u16 address) {
    // Determine if this is a background fetch BEFORE modifying state.
    // Background fetches use dedicated BG banks ($5128-$512B).
    // Sprite and other accesses use the main CHR bank mapping ($5120-$5127).
    // 
    // State machine:
    //   0 = Idle/sprite fetch (use sprite banks)
    //   1 = After NT read, before AT read (no pattern reads expected, treat as sprite)
    //   2 = After AT read, before pattern low (BG pattern fetch)
    //   3 = After pattern low, before pattern high (BG pattern fetch)
    //
    // Only states 2 and 3 are actual BG pattern fetches.
    // 
    // When outside of active rendering (!m_inFrame), use the last written register set
    // to determine which banks to use.
    bool isBgFetch;
    if (!m_inFrame) {
        // Outside of active rendering: use last written register set
        isBgFetch = (m_lastChrReg >= 0x5128 && m_lastChrReg <= 0x512B);
    } else {
        // During active rendering: use PPU fetch state
        isBgFetch = m_ppuFetchState >= 2;
    }
    
    // Check if we are in the middle of a background fetch sequence
    if (m_ppuFetchState >= 2) { // Expect Pattern Low or High
        // Handle ExRAM Mode 1 banking
        if (m_exRamMode == 1) {
            // MMC5 ExRAM Mode 1 (Extended Attributes):
            //   7..6 = palette select (AA)
            //   5..0 = 4KB CHR bank select (CCCCCC)
            // Pattern data for the current background tile comes from the 4KB bank
            // selected by bits 0-5 of the *captured* ExRAM byte.
            // Upper bits from $5130 are also used: bits 6-7 of bank come from $5130
            u32 bankIndex = (m_capturedExRam & 0x3F) | (m_chrUpperBits << 6);
            
            u32 offset = (bankIndex * 0x1000) + (address & 0x0FFF);
            
            // Advance state
            if (m_ppuFetchState == 2) m_ppuFetchState = 3;
            else m_ppuFetchState = 0; // Finished
            
            const auto& chr = m_cartridge->getCHR();
            if (offset < chr.size()) {
                return chr[offset];
            }
            return 0;
        }
        
        // Advance state for non-ExRAM modes too (to track end of fetch)
        if (m_ppuFetchState == 2) m_ppuFetchState = 3;
        else m_ppuFetchState = 0;
    }

    const auto& chr = m_cartridge->getCHR();
    if (chr.empty()) return 0;
    
    const u32* bankOffset = isBgFetch ? m_chrBgBankOffset : m_chrBankOffset;

    u8 bank = address / 0x400;
    u16 offset = address & 0x3FF;
    return chr[bankOffset[bank] + offset];
}

void Mapper005::writeCHR(u16 address, u8 value) {
    // CHR RAM support
    auto& chr = m_cartridge->getCHR();
    if (!chr.empty()) {
        // Determine which bank set to use (same logic as readCHR)
        bool isBgFetch;
        if (!m_inFrame) {
            // Outside of active rendering: use last written register set
            isBgFetch = (m_lastChrReg >= 0x5128 && m_lastChrReg <= 0x512B);
        } else {
            // During active rendering: use PPU fetch state
            isBgFetch = m_ppuFetchState >= 2;
        }
        const u32* bankOffset = isBgFetch ? m_chrBgBankOffset : m_chrBankOffset;

        u8 bank = address / 0x400;
        u16 offset = address & 0x3FF;
        chr[bankOffset[bank] + offset] = value;
    }
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
        
        // Standard modes: We must handle the read because we might have intercepted the NT read
        // and PPU expects us to return data if we returned true for NT.
        // Actually, PPU calls readNametable for each read independently.
        // So we can return false here if we want PPU to handle it via VRAM...
        // BUT if mode is CIRAM 0/1, we should probably be consistent.
        
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

MirrorMode Mapper005::getMirrorMode() const {
    return m_cartridge->getBaseMirrorMode();
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
        } else if (m_inFrame) {
            // Within frame - update scanline counter
            // Only count visible scanlines (0-239)
            if (currentScanline < 240) {
                m_scanlineCounter = currentScanline;
                
                // Check for IRQ
                if (m_scanlineCounter == m_irqScanline) {
                    m_irqStatus |= 0x80;
                    if (m_irqEnable) {
                        m_irqActive = true;
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

void Mapper005::saveState(std::ofstream& file) const {
    file.write(reinterpret_cast<const char*>(&m_prgMode), sizeof(m_prgMode));
    file.write(reinterpret_cast<const char*>(m_prgBankRegs), sizeof(m_prgBankRegs));
    file.write(reinterpret_cast<const char*>(&m_prgRamProtect1), sizeof(m_prgRamProtect1));
    file.write(reinterpret_cast<const char*>(&m_prgRamProtect2), sizeof(m_prgRamProtect2));
    file.write(reinterpret_cast<const char*>(&m_chrMode), sizeof(m_chrMode));
    file.write(reinterpret_cast<const char*>(&m_chrUpperBits), sizeof(m_chrUpperBits));
    file.write(reinterpret_cast<const char*>(m_chrBankRegs), sizeof(m_chrBankRegs));
    file.write(reinterpret_cast<const char*>(&m_lastChrReg), sizeof(m_lastChrReg));
    file.write(reinterpret_cast<const char*>(&m_nametableMapping), sizeof(m_nametableMapping));
    file.write(reinterpret_cast<const char*>(&m_fillModeTile), sizeof(m_fillModeTile));
    file.write(reinterpret_cast<const char*>(&m_fillModeAttr), sizeof(m_fillModeAttr));
    file.write(reinterpret_cast<const char*>(m_exRam.data()), m_exRam.size());
    file.write(reinterpret_cast<const char*>(&m_exRamMode), sizeof(m_exRamMode));
    file.write(reinterpret_cast<const char*>(&m_irqScanline), sizeof(m_irqScanline));
    file.write(reinterpret_cast<const char*>(&m_irqStatus), sizeof(m_irqStatus));
    file.write(reinterpret_cast<const char*>(&m_irqEnable), sizeof(m_irqEnable));
    file.write(reinterpret_cast<const char*>(&m_inFrame), sizeof(m_inFrame));
    file.write(reinterpret_cast<const char*>(&m_scanlineCounter), sizeof(m_scanlineCounter));
    file.write(reinterpret_cast<const char*>(&m_multiplicand), sizeof(m_multiplicand));
    file.write(reinterpret_cast<const char*>(&m_multiplier), sizeof(m_multiplier));
    file.write(reinterpret_cast<const char*>(m_prgRamExt.data()), m_prgRamExt.size());
    
    // New state
    file.write(reinterpret_cast<const char*>(&m_capturedExRam), sizeof(m_capturedExRam));
    file.write(reinterpret_cast<const char*>(&m_ppuFetchState), sizeof(m_ppuFetchState));
    file.write(reinterpret_cast<const char*>(&m_splitMode), sizeof(m_splitMode));
    file.write(reinterpret_cast<const char*>(&m_splitScroll), sizeof(m_splitScroll));
    file.write(reinterpret_cast<const char*>(&m_splitBank), sizeof(m_splitBank));
    file.write(reinterpret_cast<const char*>(&m_lastScanline), sizeof(m_lastScanline));
}

void Mapper005::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(&m_prgMode), sizeof(m_prgMode));
    file.read(reinterpret_cast<char*>(m_prgBankRegs), sizeof(m_prgBankRegs));
    file.read(reinterpret_cast<char*>(&m_prgRamProtect1), sizeof(m_prgRamProtect1));
    file.read(reinterpret_cast<char*>(&m_prgRamProtect2), sizeof(m_prgRamProtect2));
    file.read(reinterpret_cast<char*>(&m_chrMode), sizeof(m_chrMode));
    file.read(reinterpret_cast<char*>(&m_chrUpperBits), sizeof(m_chrUpperBits));
    file.read(reinterpret_cast<char*>(m_chrBankRegs), sizeof(m_chrBankRegs));
    file.read(reinterpret_cast<char*>(&m_lastChrReg), sizeof(m_lastChrReg));
    file.read(reinterpret_cast<char*>(&m_nametableMapping), sizeof(m_nametableMapping));
    file.read(reinterpret_cast<char*>(&m_fillModeTile), sizeof(m_fillModeTile));
    file.read(reinterpret_cast<char*>(&m_fillModeAttr), sizeof(m_fillModeAttr));
    file.read(reinterpret_cast<char*>(m_exRam.data()), m_exRam.size());
    file.read(reinterpret_cast<char*>(&m_exRamMode), sizeof(m_exRamMode));
    file.read(reinterpret_cast<char*>(&m_irqScanline), sizeof(m_irqScanline));
    file.read(reinterpret_cast<char*>(&m_irqStatus), sizeof(m_irqStatus));
    file.read(reinterpret_cast<char*>(&m_irqEnable), sizeof(m_irqEnable));
    file.read(reinterpret_cast<char*>(&m_inFrame), sizeof(m_inFrame));
    file.read(reinterpret_cast<char*>(&m_scanlineCounter), sizeof(m_scanlineCounter));
    file.read(reinterpret_cast<char*>(&m_multiplicand), sizeof(m_multiplicand));
    file.read(reinterpret_cast<char*>(&m_multiplier), sizeof(m_multiplier));
    file.read(reinterpret_cast<char*>(m_prgRamExt.data()), m_prgRamExt.size());
    
    // New state
    file.read(reinterpret_cast<char*>(&m_capturedExRam), sizeof(m_capturedExRam));
    file.read(reinterpret_cast<char*>(&m_ppuFetchState), sizeof(m_ppuFetchState));
    file.read(reinterpret_cast<char*>(&m_splitMode), sizeof(m_splitMode));
    file.read(reinterpret_cast<char*>(&m_splitScroll), sizeof(m_splitScroll));
    file.read(reinterpret_cast<char*>(&m_splitBank), sizeof(m_splitBank));
    file.read(reinterpret_cast<char*>(&m_lastScanline), sizeof(m_lastScanline));
    
    updatePRGBanks();
    updateCHRBanks();
}

} // namespace nes
