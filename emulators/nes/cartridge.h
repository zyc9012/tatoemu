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
class Mapper000;
class Mapper001;
class Mapper002;
class Mapper003;
class Mapper004;
class Mapper005;
class Mapper010;
class Mapper023;
class Mapper024;
class Mapper025;
class Mapper073;
class Mapper074;

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
    
    // CPU cycle access (for mapper timing)
    u32 getCpuCycles() const;
    
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


} // namespace nes
