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

} // namespace nes
