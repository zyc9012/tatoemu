#pragma once

#include "../../types.h"
#include "../cartridge_base.h"
#include "db.h"
#include <filesystem>
#include <string>
#include <vector>
#include <fstream>
#include <map>

namespace cps1 {

class PPU;

// CPS1 ROM/Cartridge loader
class Cartridge : public cps::CartridgeBase {
public:
    Cartridge();
    ~Cartridge() = default;

    bool load(const fs::path& filename) override;
    void reset() override;
    
    const std::string& getTitle() const override { return m_title; }
    
    // ROM access (for 68000 CPU - program ROMs)
    u8 readROM8(u32 address) override;
    u16 readROM16(u32 address) override;
    u32 readROM32(u32 address) override;
    u32 getProgramROMSize() const { return m_programRomSize; }
    
    // Graphics ROM access (for PPU)
    u8 readGraphicsROM8(u32 address) const;
    u16 readGraphicsROM16(u32 address) const;
    u32 readGraphicsROM32(u32 address) const;
    u32 getGraphicsROMSize() const { return static_cast<u32>(m_graphicsRom.size()); }
    
    // Sound ROM access (for Z80 sound CPU)
    u8 readSoundROM8(u16 address) const;
    u16 readSoundROM16(u16 address) const;
    u32 getSoundROMSize() const { return static_cast<u32>(m_soundRom.size()); }
    
    // Component connections
    void setCPU(cps::CPU* cpu) override { m_cpu = cpu; }
    void setPPU(cps::PPUBase* ppu) override;
    
    // Game info access
    const GameInfo* getGameInfo() const { return m_gameInfo; }
    
    // Board configuration access
    BoardConfig getBoardConfig() const;
    CPSBoard getBoardType() const;
    CPSMapper getMapper() const;
    
    // Save/Load state
    void saveState(std::ofstream& file) override;
    void loadState(std::ifstream& file) override;

private:
    cps::CPU* m_cpu;
    PPU* m_ppu;
    
    std::string m_title;
    std::string m_romSetName;  // MAME ROM set name (from ZIP filename)
    const GameInfo* m_gameInfo;  // Game database entry
    
    // ROM banks organized by type
    std::vector<u8> m_programRom;    // 68000 program ROMs
    std::vector<u8> m_graphicsRom;   // Graphics/tile ROMs
    std::vector<u8> m_soundRom;      // Z80 sound ROMs + ADPCM samples
    
    u32 m_programRomSize;
    u32 m_graphicsRomSize;
    u32 m_soundRomSize;
    
    bool m_programByteswap;  // Whether program ROMs need byte swapping
    
    bool loadROMsFromDatabase(const std::map<std::string, std::vector<u8>>& romFiles);
    void byteswapProgramROM();
};

} // namespace cps1
