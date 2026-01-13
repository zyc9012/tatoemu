#pragma once

#include "../../types.h"
#include "../cartridge_base.h"
#include <filesystem>
#include <string>
#include <vector>
#include <fstream>
#include <map>

namespace cps2 {

class PPU;
struct GameInfo;  // Forward declaration

// CPS2 ROM/Cartridge loader with decryption support
class Cartridge : public cps::CartridgeBase {
public:
    Cartridge();
    ~Cartridge() = default;

    bool load(const fs::path& filename) override;
    void reset() override;
    
    const std::string& getTitle() const override { return m_title; }
    
    // ROM access (for 68000 CPU - program ROMs, decrypted)
    u8 readROM8(u32 address) override;
    u16 readROM16(u32 address) override;
    u32 readROM32(u32 address) override;
    u32 getProgramROMSize() const { return m_programRomSize; }
    
    // Graphics ROM access (for PPU)
    u8 readGraphicsROM8(u32 address) const;
    u16 readGraphicsROM16(u32 address) const;
    u32 readGraphicsROM32(u32 address) const;
    u32 getGraphicsROMSize() const { return static_cast<u32>(m_graphicsRom.size()); }
    
    // Sound ROM access (for Z80 sound CPU - QSound)
    u8 readSoundROM8(u16 address) const;
    u16 readSoundROM16(u16 address) const;
    
    // Game info access
    const GameInfo* getGameInfo() const { return m_gameInfo; }
    const std::string& getRomSetName() const { return m_romSetName; }
    
    // Component connections
    void setCPU(cps::CPU* cpu) override { m_cpu = cpu; }
    void setPPU(cps::PPUBase* ppu) override;
    
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
    std::vector<u8> m_programRom;      // 68000 program ROMs (decrypted)
    std::vector<u8> m_programRomEncrypted;  // Encrypted ROMs (before decryption)
    std::vector<u8> m_graphicsRom;     // Graphics/tile ROMs
    std::vector<u8> m_soundProgramRom; // Z80 sound program ROM (QSound)
    std::vector<u8> m_soundSampleRom;  // QSound sample ROMs
    
    u32 m_programRomSize;
    u32 m_graphicsRomSize;
    u32 m_soundRomSize;
    
    // Decryption key (64-bit key for CPS2 encryption, stored as 4x32-bit)
    // This will be loaded from the game database or extracted from ROM
    // m_decryptKey[0-1] = first 64-bit key, m_decryptKey[2-3] = second 64-bit key
    u32 m_decryptKey[4];  // Two 64-bit keys stored as 4x32-bit values
    u32 m_decryptStart;   // Start address for decryption
    u32 m_decryptEnd;     // End address for decryption
    u32 m_watchdogOpcode; // Watchdog opcode (from encryption key file)
    
    bool loadROMsFromDatabase(const std::map<std::string, std::vector<u8>>& romFiles);
    void decryptProgramROM();
    void byteswapProgramROM();
};

} // namespace cps2
