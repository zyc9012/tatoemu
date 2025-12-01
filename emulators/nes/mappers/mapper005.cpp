#include "mapper005.h"
#include "../consts.h"
#include <cstring>

namespace nes {

Mapper005::Mapper005(Cartridge* cartridge)
    : Mapper(cartridge)
    , m_prgMode(3)
    , m_prgRamProtect1(false)
    , m_prgRamProtect2(false)
    , m_chrMode(0)
    , m_chrBankHigh(false)
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
    , m_multiplier(0) {
    std::memset(m_prgBankRegs, 0xFF, sizeof(m_prgBankRegs));
    std::memset(m_chrBankRegs, 0, sizeof(m_chrBankRegs));
    std::memset(m_prgBankOffset, 0, sizeof(m_prgBankOffset));
    std::memset(m_chrBankOffset, 0, sizeof(m_chrBankOffset));
    m_exRam.fill(0);
    m_prgRamExt.fill(0);
}

void Mapper005::reset() {
    m_prgMode = 3;
    m_prgRamProtect1 = false;
    m_prgRamProtect2 = false;
    m_chrMode = 0;
    m_chrBankHigh = false;
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
    
    std::memset(m_prgBankRegs, 0xFF, sizeof(m_prgBankRegs));
    std::memset(m_chrBankRegs, 0, sizeof(m_chrBankRegs));
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
    
    switch (m_chrMode) {
        case 0:  // 8KB mode
            {
                u16 bank = m_chrBankRegs[7] & 0xFF;
                u32 offset = (bank % (chrBanks1k / 8)) * 0x2000;
                for (int i = 0; i < 8; i++) {
                    m_chrBankOffset[i] = offset + i * 0x400;
                }
            }
            break;
            
        case 1:  // 4KB mode
            {
                u16 bank0 = m_chrBankRegs[3] & 0xFF;
                u16 bank1 = m_chrBankRegs[7] & 0xFF;
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
                u16 bank0 = m_chrBankRegs[1] & 0xFF;
                u16 bank1 = m_chrBankRegs[3] & 0xFF;
                u16 bank2 = m_chrBankRegs[5] & 0xFF;
                u16 bank3 = m_chrBankRegs[7] & 0xFF;
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
                u16 bank = m_chrBankRegs[i] & 0xFF;
                m_chrBankOffset[i] = (bank % chrBanks1k) * 0x400;
            }
            break;
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
                // CHR banks (sprite)
                m_chrBankRegs[address - 0x5120] = value;
                m_chrBankHigh = false;
                updateCHRBanks();
                break;
            case 0x5128: case 0x5129: case 0x512A: case 0x512B:
                // CHR banks (background)
                m_chrBankRegs[8 + (address - 0x5128)] = value;
                m_chrBankHigh = true;
                updateCHRBanks();
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
    const auto& chr = m_cartridge->getCHR();
    if (chr.empty()) return 0;
    
    u8 bank = address / 0x400;
    u16 offset = address & 0x3FF;
    return chr[m_chrBankOffset[bank] + offset];
}

void Mapper005::writeCHR(u16 address, u8 value) {
    // CHR RAM support
    auto& chr = m_cartridge->getCHR();
    if (!chr.empty()) {
        u8 bank = address / 0x400;
        u16 offset = address & 0x3FF;
        chr[m_chrBankOffset[bank] + offset] = value;
    }
}

MirrorMode Mapper005::getMirrorMode() const {
    // MMC5 has complex nametable mapping, simplified to basic modes
    return m_cartridge->getBaseMirrorMode();
}

void Mapper005::scanlineCounter() {
    if (!m_inFrame) {
        m_inFrame = true;
        m_scanlineCounter = 0;
    }
    
    m_scanlineCounter++;
    
    if (m_scanlineCounter == m_irqScanline) {
        m_irqStatus |= 0x80;  // Set pending
        if (m_irqEnable) {
            m_irqActive = true;
        }
    }
    
    // Detect end of frame (scanline 240)
    if (m_scanlineCounter >= 240) {
        m_inFrame = false;
        m_irqStatus &= ~0x40;  // Clear in-frame
    } else {
        m_irqStatus |= 0x40;   // Set in-frame
    }
}

void Mapper005::saveState(std::ofstream& file) const {
    file.write(reinterpret_cast<const char*>(&m_prgMode), sizeof(m_prgMode));
    file.write(reinterpret_cast<const char*>(m_prgBankRegs), sizeof(m_prgBankRegs));
    file.write(reinterpret_cast<const char*>(&m_prgRamProtect1), sizeof(m_prgRamProtect1));
    file.write(reinterpret_cast<const char*>(&m_prgRamProtect2), sizeof(m_prgRamProtect2));
    file.write(reinterpret_cast<const char*>(&m_chrMode), sizeof(m_chrMode));
    file.write(reinterpret_cast<const char*>(m_chrBankRegs), sizeof(m_chrBankRegs));
    file.write(reinterpret_cast<const char*>(&m_chrBankHigh), sizeof(m_chrBankHigh));
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
}

void Mapper005::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(&m_prgMode), sizeof(m_prgMode));
    file.read(reinterpret_cast<char*>(m_prgBankRegs), sizeof(m_prgBankRegs));
    file.read(reinterpret_cast<char*>(&m_prgRamProtect1), sizeof(m_prgRamProtect1));
    file.read(reinterpret_cast<char*>(&m_prgRamProtect2), sizeof(m_prgRamProtect2));
    file.read(reinterpret_cast<char*>(&m_chrMode), sizeof(m_chrMode));
    file.read(reinterpret_cast<char*>(m_chrBankRegs), sizeof(m_chrBankRegs));
    file.read(reinterpret_cast<char*>(&m_chrBankHigh), sizeof(m_chrBankHigh));
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
    updatePRGBanks();
    updateCHRBanks();
}

} // namespace nes

