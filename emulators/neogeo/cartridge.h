#pragma once

#include "../types.h"
#include "db.h"
#include <filesystem>
#include <string>
#include <vector>
#include <map>
#include <fstream>

namespace neogeo {

class CPU;
class PPU;

// System types
enum class SystemType {
    AES,  // Advanced Entertainment System (home console)
    MVS   // Multi Video System (arcade)
};

// NeoGeo Cartridge loader
// Loads ROMs from ZIP files (MAME format)
// For now, skips decryption as requested
class Cartridge {
public:
    Cartridge();
    ~Cartridge() = default;

    bool load(const fs::path& filename, u32 bios68kIndex = 0);
    void reset();
    
    const std::string& getTitle() const { return m_title; }
    const fs::path& getRomFilename() const { return m_romFilename; }

    // Game info access
    const GameInfo* getGameInfo() const { return m_gameInfo; }
    
    // ROM access (for 68000 CPU - program ROMs)
    u8 readROM8(u32 address);
    u16 readROM16(u32 address);
    u32 getProgramROMSize() const { return static_cast<u32>(m_programRom.size()); }
    
    // Graphics ROM access (for PPU - sprite ROMs)
    u8 readSpriteROM8(u32 address) const;
    u32 getSpriteROMSize() const { return static_cast<u32>(m_spriteRom.size()); }
    
    // Text ROM access (for PPU)
    u8 readTextROM8(u32 address) const;
    u32 getTextROMSize() const { return static_cast<u32>(m_textRom.size()); }
    
    // BIOS ROM access
    u8 readBIOS68K8(u32 address) const;
    u8 readBIOSZ808(u32 address) const;
    u8 readBIOSText8(u32 address) const;
    u8 readZoomROM8(u32 address) const;
    
    // Vector table access
    u8 readVectorTable8(u32 address) const;  // 0x000000-0x0003FF
    u8 readBiosVectorTable8(u32 address) const;  // 0xC00000-0xC003FF
    void setBiosVectorTableActive(bool active);  // Switch between BIOS/cartridge vector table
    
    // Program ROM access
    u8 readProgramROM8(u32 offset) const;  // Raw ROM access by offset
    
    // Sound ROM access (for Z80 - M ROM)
    u8 readSoundROM8(u32 address) const;
    u32 getSoundROMSize() const { return static_cast<u32>(m_soundRom.size()); }
    
    // ADPCM ROM access (for YM2610 - V ROMs)
    const u8* getADPCMROM() const { return m_adpcmRom.empty() ? nullptr : m_adpcmRom.data(); }
    u32 getADPCMROMSize() const { return static_cast<u32>(m_adpcmRom.size()); }
    
    // Component connections
    void setCPU(CPU* cpu) { m_cpu = cpu; }
    void setPPU(PPU* ppu) { m_ppu = ppu; }

    // Save/Load state
    void saveState(std::ofstream& file);
    void loadState(std::ifstream& file);

private:
    CPU* m_cpu;
    PPU* m_ppu;
    
    std::string m_title;
    std::string m_romSetName;  // MAME ROM set name (from ZIP filename)
    fs::path m_romFilename;     // Full path to ROM file
    const GameInfo* m_gameInfo;  // Game database entry
    
    // ROM banks organized by type
    std::vector<u8> m_programRom;      // 68000 program ROMs (P ROMs)
    std::vector<u8> m_spriteRom;        // Sprite/graphics ROMs (C ROMs)
    std::vector<u8> m_textRom;         // Text layer ROM (S ROM)
    std::vector<u8> m_soundRom;        // Z80 sound program ROM (M ROM)
    std::vector<u8> m_adpcmRom;        // ADPCM sample ROMs (V ROMs)
    
    // BIOS ROMs
    std::vector<u8> m_bios68kRom;      // 68000 BIOS ROM (0x80000 bytes)
    std::vector<u8> m_biosZ80Rom;      // Z80 BIOS ROM (0x20000 bytes)
    std::vector<u8> m_biosTextRom;     // Text BIOS ROM (0x20000 bytes)
    std::vector<u8> m_zoomRom;         // Zoom table ROM (0x20000 bytes)
    
    // Vector tables (0x400 bytes each)
    std::vector<u8> m_hybridBiosVectors;   // BIOS[0x00-0x7F] + Cart[0x80-0x3FF] - used at 0x000000 when BIOS vectors active
    std::vector<u8> m_hybridCartVectors;   // Cart[0x00-0x7F] + BIOS[0x80-0x3FF] - used at 0xC00000 when BIOS vectors active
    bool m_biosVectorTableActive;          // true = BIOS vectors at 0x000000, false = cartridge vectors
    
    u32 m_programRomSize;
    u32 m_spriteRomSize;
    u32 m_textRomSize;
    u32 m_soundRomSize;
    
    // Helper to load ROMs from database
    bool loadROMsFromDatabase(const std::map<std::string, std::vector<u8>>& romFiles);
    bool loadBIOSROMs(const std::map<std::string, std::vector<u8>>& romFiles, const fs::path& gameRomPath, u32 bios68kIndex);
    
    // ROM decoding functions
    void decodeTextROM();
    void decodeSpriteROM();
    bool interleaveSpriteROMs(const std::vector<std::vector<u8>>& spriteRomChips);
    void byteswapBiosROM();
    void byteswapProgramROM();
    void decodeBIOSTextROM();
    void processSWAPCRom();  // SWAPC: swap sprite ROM regions
    void buildVectorTables();  // Build vector table copies
    void extractTextFromSprites(u32 textRomSize);  // Extract text data from sprite ROMs

    // Text tile decoding helper (decodes one 32-byte tile)
    static void decodeTextTile(const u8* src, u8* dst);
};

} // namespace neogeo
