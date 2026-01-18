#pragma once

#include "../types.h"
#include <string>
#include <vector>

namespace neogeo {

// ROM entry types
enum class ROMType {
    PROGRAM,        // 68000 program ROM (P ROMs)
    TEXT,           // Text layer ROM (S ROM)
    SPRITE,         // Sprite/graphics ROM (C ROMs)
    SOUND_PROGRAM,  // Z80 sound program ROM (M ROM)
    SOUND_SAMPLE,   // ADPCM sample ROMs (V ROMs)
    UNKNOWN
};

// ROM file entry
struct ROMEntry {
    const char* filename;
    u32 size;
    u32 crc32;
    ROMType type;
    u8 flags;  // Reserved for future use
};

// BIOS ROM entry (for neogeo.zip)
struct BIOSROMEntry {
    const char* filename;
    u32 size;
    u32 crc32;
    u8 flags;  // BRF_BIOS flag equivalent
};

// Game hardware flags
enum GameFlags {
    GAME_FLAG_SWAPP = 0x01,           // SWAPP: Swap code roms (first half/second half of first P ROM)
    GAME_FLAG_SWAPV = 0x02,           // SWAPV: Swap sound roms
    GAME_FLAG_SWAPC = 0x04,           // SWAPC: Swap sprite roms
    GAME_FLAG_CMC42 = 0x08,           // CMC42: CMC42 encryption chip
    GAME_FLAG_CMC50 = 0x10,           // CMC50: CMC50 encryption chip
    GAME_FLAG_ALTERNATE_TEXT = 0x20,  // ALTERNATE_TEXT: KOF2000 text layer banks
    GAME_FLAG_SMA_PROTECTION = 0x40,  // SMA_PROTECTION: SMA protection
    GAME_FLAG_KOF2K3 = 0x80,          // KOF2K3: KOF2003 hardware
    GAME_FLAG_ENCRYPTED_M1 = 0x100,   // ENCRYPTED_M1: M1 encryption
    GAME_FLAG_P32 = 0x200,            // P32: SWAP32 P ROMs (32-bit program ROM interleaving)
    GAME_FLAG_SPRITE32 = 0x400,       // SPRITE32: Sprite32
};

// Game database entry
struct GameInfo {
    const char* name;              // Game name
    const char* romSetName;          // MAME ROM set name
    const ROMEntry* roms;          // Array of ROM entries
    u32 romCount;                  // Number of ROM entries
    u8 flags;                      // Hardware flags (P32, SWAPP, etc.)
};

// Game database
class GameDatabase {
public:
    static const GameInfo* findGame(const std::string& romSetName);
    static bool validateROM(const std::string& filename, const std::vector<u8>& data, const ROMEntry& entry);
    static bool validateBIOSROM(const std::string& filename, const std::vector<u8>& data, const BIOSROMEntry& entry);
    static u32 calculateCRC32(const std::vector<u8>& data);
    
    // BIOS ROM definitions
    static const BIOSROMEntry* getBIOSROMs();
    static u32 getBIOSROMCount();
    
private:
    static const GameInfo s_games[];
    static const u32 s_gameCount;
    static const BIOSROMEntry s_biosROMs[];
    static const u32 s_biosROMCount;
};

} // namespace neogeo
