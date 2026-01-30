#pragma once

#include "../types.h"
#include "db.h"
#include "../components/buffer.h"
#include <filesystem>
#include <string>
#include <vector>
#include <map>

namespace cps {

class CPU;
class PPU;

// Forward declarations - PPU is now unified

// Unified CPS ROM/Cartridge loader supporting both CPS1 and CPS2
class Cartridge {
public:
    Cartridge();
    ~Cartridge() = default;

    bool load(const fs::path& filename);
    void reset();
    
    const std::string& getTitle() const { return m_title; }
    
    // ROM access (for 68000 CPU - program ROMs)
    u8 readROM8(u32 address);
    u16 readROM16(u32 address);
    u32 getProgramROMSize() const { return m_programRomSize; }
    
    // Encrypted ROM access (for data reads in CPS2 - exception vectors should use this)
    u8 readEncryptedROM8(u32 address);
    u16 readEncryptedROM16(u32 address);
    
    // Graphics ROM access (for PPU)
    u8 readGraphicsROM8(u32 address) const;
    u32 getGraphicsROMSize() const { return static_cast<u32>(m_graphicsRom.size()); }
    
    // Decoded graphics ROM access (for PPU)
    const u8* getDecodedGraphicsROM() const { return m_decodedGraphicsRom.data(); }
    u32 getDecodedGraphicsROMSize() const { return static_cast<u32>(m_decodedGraphicsRom.size()); }
    
    // Sound ROM access (for Z80 sound CPU)
    u8 readSoundROM8(u32 address) const;
    u8 readEncryptedSoundROM8(u32 address) const;
    u32 getSoundROMSize() const { return static_cast<u32>(m_soundProgramRom.size()); }
    
    // Sound sample data access
    const u8* getSoundSample() const { return m_soundSampleRom.data(); }
    u32 getSoundSampleSize() const { return static_cast<u32>(m_soundSampleRom.size()); }
    
    // Component connections
    void setCPU(CPU* cpu) { m_cpu = cpu; }
    void setPPU(PPU* ppu);
    
    // CPS version
    u8 getCPSVersion() const { return m_cpsVer; }
    void setCPSVersion(u8 version) { m_cpsVer = version; }

    // QSound detection
    bool isCPS1QSound() const { return m_gameInfo && (m_gameInfo->flags & GAME_FLAG_CPS1_QSOUND); }
    
    // Game info access
    const GameInfo* getGameInfo() const { return m_gameInfo; }
    const std::string& getRomSetName() const { return m_romSetName; }
    
    // CPS1-specific board configuration access
    BoardConfig getBoardConfig() const;
    CPSBoard getBoardType() const;
    CPSMapper getMapper() const;

    // Save/Load state
    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

private:
    CPU* m_cpu;
    PPU* m_ppu;
    
    u8 m_cpsVer = 1;  // CPS version: 1 for CPS1, 2 for CPS2
    
    std::string m_title;
    std::string m_romSetName;  // MAME ROM set name (from ZIP filename)
    const GameInfo* m_gameInfo;  // Game database entry
    
    // ROM banks organized by type
    std::vector<u8> m_programRom;      // 68000 program ROMs
    std::vector<u8> m_programRomEncrypted;  // Encrypted ROMs (CPS2 only, before decryption)
    std::vector<u8> m_graphicsRom;     // Graphics/tile ROMs
    std::vector<u8> m_decodedGraphicsRom;  // Decoded graphics ROM
    std::vector<u8> m_soundProgramRom; // Z80 sound program ROM
    std::vector<u8> m_soundProgramRomEncrypted; // Encrypted Z80 sound program ROM (CPS1 QSound only)
    std::vector<u8> m_soundSampleRom;  // ADPCM sample ROMs (CPS1) or QSound samples (CPS2)
    
    u32 m_programRomSize;
    u32 m_graphicsRomSize;
    u32 m_soundProgramRomSize;
    u32 m_soundSampleRomSize;
    
    // CPS2 decryption key (64-bit key for CPS2 encryption, stored as 4x32-bit)
    // m_decryptKey[0-1] = first 64-bit key, m_decryptKey[2-3] = second 64-bit key
    u32 m_decryptKey[4];  // Two 64-bit keys stored as 4x32-bit values
    u32 m_decryptStart;   // Start address for decryption
    u32 m_decryptEnd;     // End address for decryption
    
    // Separation table for graphics decoding
    // Converts a byte to spread-out bits: ABCDEFGH -> A00B00C00D00E00F00G00H00
    u32 m_sepTable[256];
    
    void initSepTable();
    bool loadROMsFromDatabase(const std::map<std::string, std::vector<u8>>& romFiles);
    void decryptCPS1SoundProgramROM();
    void decryptCPS2ProgramROM();
    void byteswap(std::vector<u8>& rom);
    void interleavedCopy(u8* dest, const u8* src, u32 size);
    u32 calcGraphicsROMSizeFix(const GameInfo* gameInfo);
    void decodeGraphicsROM(const std::vector<u32>& graphicsRomSizes = {});
    void decodeGraphicsROMCPS1(const std::vector<u32>& graphicsRomSizes);
    void decodeGraphicsROMCPS2(const std::vector<u32>& graphicsRomSizes);
};

} // namespace cps
