#pragma once

#include "../../types.h"
#include <string>
#include <vector>

namespace cps2 {

// ROM entry types
enum class ROMType {
    PROGRAM,      // 68000 program ROM (encrypted)
    GRAPHICS,     // Graphics/tile ROM
    SOUND_PROGRAM,// Z80 sound program ROM (QSound)
    SOUND_SAMPLE, // QSound sample ROMs
    ENCRYPTION_KEY, // Decryption key (64-bit key + watchdog opcode)
    UNKNOWN
};

// ROM entry flags
enum ROMFlags {
    ROM_FLAG_OPTIONAL = 0x02,      // Optional ROM
};

// ROM file entry
struct ROMEntry {
    const char* filename;
    u32 size;
    u32 crc32;
    ROMType type;
    u8 flags;
};

// Game database entry
struct GameInfo {
    const char* name;              // Game name
    const char* romSetName;        // MAME ROM set name
    const ROMEntry* roms;          // Array of ROM entries
    u32 romCount;                  // Number of ROM entries
    
    // Decryption key (64-bit key stored as two 32-bit values)
    u32 decryptKey[4];             // 128 bits total (two 64-bit keys)
    u32 decryptStart;               // Start address for decryption
    u32 decryptEnd;                 // End address for decryption
    u32 watchdogOpcode;            // Watchdog opcode (some games)
};

// Game database
class GameDatabase {
public:
    static const GameInfo* findGame(const std::string& romSetName);
    static bool validateROM(const std::string& filename, const std::vector<u8>& data, const ROMEntry& entry);
    static u32 calculateCRC32(const std::vector<u8>& data);
    
private:
    static const GameInfo s_games[];
    static const u32 s_gameCount;
};

} // namespace cps2
