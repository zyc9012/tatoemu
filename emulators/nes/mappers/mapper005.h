#pragma once

#include "../cartridge.h"
#include <array>

namespace nes {

// Mapper 5: MMC5 (ExROM) - Most complex NES mapper
class Mapper005 : public Mapper {
public:
    Mapper005(Cartridge* cartridge);
    void reset() override;
    
    u8 cpuRead(u16 address) override;
    void cpuWrite(u16 address, u8 value) override;
    u8 readCHR(u16 address) override;
    void writeCHR(u16 address, u8 value) override;

    bool readNametable(u16 address, u8& value) override;
    bool writeNametable(u16 address, u8 value) override;
    
    MirrorMode getMirrorMode() const override;
    void scanlineCounter() override;
    
    void saveState(std::ofstream& file) const override;
    void loadState(std::ifstream& file) override;
    
private:
    void updatePRGBanks();
    void updateCHRBanks();
    u8 readExRAM(u16 address);
    void writeExRAM(u16 address, u8 value);
    
    // PRG mode and banks
    u8 m_prgMode;           // PRG banking mode (0-3)
    u8 m_prgBankRegs[5];    // PRG bank registers
    u32 m_prgBankOffset[4]; // Calculated PRG offsets
    bool m_prgRamProtect1;
    bool m_prgRamProtect2;
    
    // CHR mode and banks
    u8 m_chrMode;           // CHR banking mode (0-3)
    u8 m_chrUpperBits;      // CHR upper bits from $5130 (bits 8-9 of 10-bit banks)
    u16 m_chrBankRegs[12];  // CHR bank registers (extended to 10 bits)
    u32 m_chrBankOffset[8]; // Calculated CHR offsets
    u32 m_chrBgBankOffset[8]; // Calculated CHR offsets for background fetches
    u16 m_lastChrReg;       // Last written CHR register (0x5120-0x5127 for sprite, 0x5128-0x512B for BG)
    
    // Nametable mapping
    u8 m_nametableMapping;
    u8 m_fillModeTile;
    u8 m_fillModeAttr;
    
    // Extended RAM (1KB)
    std::array<u8, 1024> m_exRam;
    u8 m_exRamMode;
    
    // IRQ
    u8 m_irqScanline;
    u8 m_irqStatus;
    bool m_irqEnable;
    bool m_inFrame;
    u8 m_scanlineCounter;
    
    // Multiplier
    u8 m_multiplicand;
    u8 m_multiplier;
    
    // Additional RAM (up to 64KB)
    std::array<u8, 0x10000> m_prgRamExt;

    // PPU Fetch State
    u8 m_capturedExRam;
    u8 m_ppuFetchState; // 0: Idle, 1: Attr, 2: PatternLow, 3: PatternHigh
    
    // Split screen registers ($5200-$5202)
    u8 m_splitMode;
    u8 m_splitScroll;
    u8 m_splitBank;
    
    // Internal tracking for scanline detection
    u16 m_lastScanline;     // Last scanline we processed (to detect changes)
};

} // namespace nes

