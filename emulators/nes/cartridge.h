#pragma once

#include "../types.h"
#include "consts.h"
#include <filesystem>
#include <string>
#include <vector>
#include <memory>
#include <fstream>

namespace nes {

class CPU;
class PPU;

// Forward declarations for mappers
class Mapper;

class Cartridge {
public:
    Cartridge();
    ~Cartridge();

    void setCPU(CPU* cpu) { m_cpu = cpu; }
    void setPPU(PPU* ppu) { m_ppu = ppu; }
    
    bool load(const fs::path& filename);
    void reset();
    
    // CPU bus access ($4020-$FFFF)
    u8 cpuRead(u16 address);
    void cpuWrite(u16 address, u8 value);
    
    // PPU bus access (CHR)
    u8 readCHR(u16 address);
    void writeCHR(u16 address, u8 value);
    
    // Mirroring
    MirrorMode getMirrorMode() const;
    MirrorMode getBaseMirrorMode() const { return m_mirrorMode; }  // Direct access without mapper delegation
    void setMirrorMode(MirrorMode mode);
    
    // Mapper IRQ (for MMC3 etc.)
    void scanlineCounter();
    bool irqState() const;
    void irqClear();
    
    // Expansion audio (for VRC6, VRC7, etc.)
    void clockAudio();
    float getExpansionAudio() const;
    bool hasExpansionAudio() const;
    
    // ROM info
    bool isLoaded() const { return m_loaded; }
    const std::string& getTitle() const { return m_title; }
    u8 getMapperNumber() const { return m_mapperNumber; }
    
    // Raw ROM access (for mappers)
    std::vector<u8>& getPRG() { return m_prgRom; }
    std::vector<u8>& getCHR() { return m_chrRom; }
    std::vector<u8>& getPRGRAM() { return m_prgRam; }
    const std::vector<u8>& getPRG() const { return m_prgRom; }
    const std::vector<u8>& getCHR() const { return m_chrRom; }
    const std::vector<u8>& getPRGRAM() const { return m_prgRam; }
    
    // Save/Load state
    void saveState(std::ofstream& file) const;
    void loadState(std::ifstream& file);
    
    // Battery save/load
    void saveBattery() const;
    void loadBattery();
    bool hasBattery() const { return m_hasBattery; }

private:
    bool parseINES(const std::vector<u8>& data);
    void createMapper();
    
    CPU* m_cpu;
    PPU* m_ppu;
    
    std::unique_ptr<Mapper> m_mapper;
    
    std::vector<u8> m_prgRom;    // PRG ROM
    std::vector<u8> m_chrRom;    // CHR ROM (or RAM)
    std::vector<u8> m_prgRam;    // PRG RAM (battery-backed SRAM)
    
    std::string m_title;
    fs::path m_romFilename;
    
    u8 m_mapperNumber;
    u8 m_prgBanks;               // Number of 16KB PRG ROM banks
    u8 m_chrBanks;               // Number of 8KB CHR ROM banks
    MirrorMode m_mirrorMode;
    bool m_hasBattery;
    bool m_hasTrainer;
    bool m_loaded;
};

// Base mapper class
class Mapper {
public:
    Mapper(Cartridge* cartridge) : m_cartridge(cartridge), m_irqActive(false) {}
    virtual ~Mapper() = default;
    
    virtual void reset() = 0;
    
    virtual u8 cpuRead(u16 address) = 0;
    virtual void cpuWrite(u16 address, u8 value) = 0;
    
    virtual u8 readCHR(u16 address) = 0;
    virtual void writeCHR(u16 address, u8 value) = 0;
    
    virtual MirrorMode getMirrorMode() const { return m_cartridge->getBaseMirrorMode(); }
    virtual void setMirrorMode(MirrorMode mode) { m_cartridge->setMirrorMode(mode); }
    
    // Scanline counter (for MMC3)
    virtual void scanlineCounter() {}
    
    // IRQ handling
    virtual bool irqState() const { return m_irqActive; }
    virtual void irqClear() { m_irqActive = false; }
    
    // Expansion audio (for VRC6, VRC7, etc.)
    virtual void clockAudio() {}
    virtual float getAudioOutput() const { return 0.0f; }
    virtual bool hasExpansionAudio() const { return false; }
    
    // Save/Load state
    virtual void saveState(std::ofstream& file) const = 0;
    virtual void loadState(std::ifstream& file) = 0;
    
protected:
    Cartridge* m_cartridge;
    bool m_irqActive;
};

// Mapper 0: NROM - No mapper (simple ROM)
class Mapper000 : public Mapper {
public:
    Mapper000(Cartridge* cartridge);
    void reset() override;
    
    u8 cpuRead(u16 address) override;
    void cpuWrite(u16 address, u8 value) override;
    u8 readCHR(u16 address) override;
    void writeCHR(u16 address, u8 value) override;
    
    void saveState(std::ofstream& file) const override;
    void loadState(std::ifstream& file) override;
};

// Mapper 1: MMC1 (SxROM)
class Mapper001 : public Mapper {
public:
    Mapper001(Cartridge* cartridge);
    void reset() override;
    
    u8 cpuRead(u16 address) override;
    void cpuWrite(u16 address, u8 value) override;
    u8 readCHR(u16 address) override;
    void writeCHR(u16 address, u8 value) override;
    
    MirrorMode getMirrorMode() const override;
    
    void saveState(std::ofstream& file) const override;
    void loadState(std::ifstream& file) override;
    
private:
    void updateBanks();
    
    u8 m_shiftRegister;
    u8 m_shiftCount;
    
    u8 m_control;       // $8000-$9FFF
    u8 m_chrBank0;      // $A000-$BFFF
    u8 m_chrBank1;      // $C000-$DFFF
    u8 m_prgBank;       // $E000-$FFFF
    
    u32 m_prgBankOffset[2];
    u32 m_chrBankOffset[2];
};

// Mapper 2: UxROM
class Mapper002 : public Mapper {
public:
    Mapper002(Cartridge* cartridge);
    void reset() override;
    
    u8 cpuRead(u16 address) override;
    void cpuWrite(u16 address, u8 value) override;
    u8 readCHR(u16 address) override;
    void writeCHR(u16 address, u8 value) override;
    
    void saveState(std::ofstream& file) const override;
    void loadState(std::ifstream& file) override;
    
private:
    u8 m_prgBank;
};

// Mapper 3: CNROM
class Mapper003 : public Mapper {
public:
    Mapper003(Cartridge* cartridge);
    void reset() override;
    
    u8 cpuRead(u16 address) override;
    void cpuWrite(u16 address, u8 value) override;
    u8 readCHR(u16 address) override;
    void writeCHR(u16 address, u8 value) override;
    
    void saveState(std::ofstream& file) const override;
    void loadState(std::ifstream& file) override;
    
private:
    u8 m_chrBank;
};

// Mapper 4: MMC3 (TxROM)
class Mapper004 : public Mapper {
public:
    Mapper004(Cartridge* cartridge);
    void reset() override;
    
    u8 cpuRead(u16 address) override;
    void cpuWrite(u16 address, u8 value) override;
    u8 readCHR(u16 address) override;
    void writeCHR(u16 address, u8 value) override;
    
    MirrorMode getMirrorMode() const override;
    void scanlineCounter() override;
    
    void saveState(std::ofstream& file) const override;
    void loadState(std::ifstream& file) override;
    
private:
    void updateBanks();
    
    u8 m_bankSelect;
    u8 m_bankData[8];
    
    u8 m_irqLatch;
    u8 m_irqCounter;
    bool m_irqEnable;
    bool m_irqReload;
    
    MirrorMode m_mirrorMode;
    bool m_prgRamEnable;
    
    u32 m_prgBankOffset[4];
    u32 m_chrBankOffset[8];
};

// Mapper 10: MMC4 (FxROM)
class Mapper010 : public Mapper {
public:
    Mapper010(Cartridge* cartridge);
    void reset() override;
    
    u8 cpuRead(u16 address) override;
    void cpuWrite(u16 address, u8 value) override;
    u8 readCHR(u16 address) override;
    void writeCHR(u16 address, u8 value) override;
    
    MirrorMode getMirrorMode() const override;
    
    void saveState(std::ofstream& file) const override;
    void loadState(std::ifstream& file) override;
    
private:
    u8 m_prgBank;
    u8 m_chrBank0FD;
    u8 m_chrBank0FE;
    u8 m_chrBank1FD;
    u8 m_chrBank1FE;
    u8 m_latch0;
    u8 m_latch1;
    MirrorMode m_mirrorMode;
};

// Mapper 5: MMC5 (ExROM) - Most complex NES mapper
class Mapper005 : public Mapper {
public:
    Mapper005(Cartridge* cartridge);
    void reset() override;
    
    u8 cpuRead(u16 address) override;
    void cpuWrite(u16 address, u8 value) override;
    u8 readCHR(u16 address) override;
    void writeCHR(u16 address, u8 value) override;
    
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
    u16 m_chrBankRegs[12];  // CHR bank registers (extended to 10 bits)
    u32 m_chrBankOffset[8]; // Calculated CHR offsets
    bool m_chrBankHigh;     // High CHR bank select (for sprite/bg separation)
    
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
};

// Mapper 23: VRC2b / VRC4e / VRC4f (Konami)
class Mapper023 : public Mapper {
public:
    Mapper023(Cartridge* cartridge);
    void reset() override;
    
    u8 cpuRead(u16 address) override;
    void cpuWrite(u16 address, u8 value) override;
    u8 readCHR(u16 address) override;
    void writeCHR(u16 address, u8 value) override;
    
    MirrorMode getMirrorMode() const override;
    void scanlineCounter() override;
    
    void saveState(std::ofstream& file) const override;
    void loadState(std::ifstream& file) override;
    
private:
    void updateBanks();
    
    u8 m_prgBank[2];        // 8KB PRG banks
    u8 m_chrBank[8];        // 1KB CHR banks (low nibbles)
    u8 m_chrBankHigh[8];    // 1KB CHR banks (high nibbles)
    u8 m_prgSwapMode;       // PRG swap mode
    MirrorMode m_mirrorMode;
    
    // IRQ
    u8 m_irqLatch;
    u8 m_irqCounter;
    u8 m_irqPrescaler;
    u16 m_irqPrescalerCounter;
    bool m_irqEnable;
    bool m_irqEnableOnAck;
    bool m_irqMode;         // 0 = scanline, 1 = cycle
    
    u32 m_prgBankOffset[4];
    u32 m_chrBankOffset[8];
};

// VRC6 Audio Pulse Channel
struct VRC6Pulse {
    u8 volume;          // 4-bit volume (0-15)
    u8 duty;            // 3-bit duty cycle (0-7)
    u16 period;         // 12-bit period
    u16 timer;          // Current timer counter
    u8 step;            // Current duty cycle step (0-15)
    bool enabled;       // Channel enabled
    bool mode;          // Mode flag (constant volume)
    
    void reset();
    void clockTimer();
    u8 output() const;
};

// VRC6 Audio Sawtooth Channel
struct VRC6Sawtooth {
    u8 accumRate;       // 6-bit accumulator rate
    u16 period;         // 12-bit period
    u16 timer;          // Current timer counter
    u8 accumulator;     // 8-bit accumulator
    u8 step;            // Step counter (0-13, reset to 0 on 14)
    bool enabled;       // Channel enabled
    
    void reset();
    void clockTimer();
    u8 output() const;
};

// Mapper 24: VRC6a (Konami with extra audio)
class Mapper024 : public Mapper {
public:
    Mapper024(Cartridge* cartridge);
    void reset() override;
    
    u8 cpuRead(u16 address) override;
    void cpuWrite(u16 address, u8 value) override;
    u8 readCHR(u16 address) override;
    void writeCHR(u16 address, u8 value) override;
    
    MirrorMode getMirrorMode() const override;
    void scanlineCounter() override;
    
    // VRC6 Audio
    void clockAudio() override;
    float getAudioOutput() const override;
    bool hasExpansionAudio() const override { return true; }
    
    void saveState(std::ofstream& file) const override;
    void loadState(std::ifstream& file) override;
    
private:
    void updateBanks();
    
    u8 m_prgBank16k;        // 16KB PRG bank at $8000
    u8 m_prgBank8k;         // 8KB PRG bank at $C000
    u8 m_chrBank[8];        // 1KB CHR banks
    MirrorMode m_mirrorMode;
    
    // IRQ (same as VRC4)
    u8 m_irqLatch;
    u8 m_irqCounter;
    u8 m_irqPrescaler;
    u16 m_irqPrescalerCounter;
    bool m_irqEnable;
    bool m_irqEnableOnAck;
    bool m_irqMode;
    
    u32 m_prgBankOffset[4];
    u32 m_chrBankOffset[8];
    
    // VRC6 Audio channels
    VRC6Pulse m_vrcPulse1;
    VRC6Pulse m_vrcPulse2;
    VRC6Sawtooth m_vrcSaw;
    bool m_audioHalt;       // Global halt flag
};

// Mapper 25: VRC4b / VRC4d (Konami) - Similar to 23 with different address lines
class Mapper025 : public Mapper {
public:
    Mapper025(Cartridge* cartridge);
    void reset() override;
    
    u8 cpuRead(u16 address) override;
    void cpuWrite(u16 address, u8 value) override;
    u8 readCHR(u16 address) override;
    void writeCHR(u16 address, u8 value) override;
    
    MirrorMode getMirrorMode() const override;
    void scanlineCounter() override;
    
    void saveState(std::ofstream& file) const override;
    void loadState(std::ifstream& file) override;
    
private:
    void updateBanks();
    
    u8 m_prgBank[2];
    u8 m_chrBank[8];
    u8 m_chrBankHigh[8];
    u8 m_prgSwapMode;
    MirrorMode m_mirrorMode;
    
    // IRQ
    u8 m_irqLatch;
    u8 m_irqCounter;
    u8 m_irqPrescaler;
    u16 m_irqPrescalerCounter;
    bool m_irqEnable;
    bool m_irqEnableOnAck;
    bool m_irqMode;
    
    u32 m_prgBankOffset[4];
    u32 m_chrBankOffset[8];
};

// Mapper 73: VRC3 (Konami) - Salamander
class Mapper073 : public Mapper {
public:
    Mapper073(Cartridge* cartridge);
    void reset() override;
    
    u8 cpuRead(u16 address) override;
    void cpuWrite(u16 address, u8 value) override;
    u8 readCHR(u16 address) override;
    void writeCHR(u16 address, u8 value) override;
    
    void scanlineCounter() override;
    
    void saveState(std::ofstream& file) const override;
    void loadState(std::ifstream& file) override;
    
private:
    u8 m_prgBank;           // 16KB PRG bank
    
    // IRQ
    u16 m_irqLatch;
    u16 m_irqCounter;
    bool m_irqEnable;
    bool m_irqEnableOnAck;
    bool m_irqMode;         // 0 = 16-bit, 1 = 8-bit (high byte only)
};

} // namespace nes
