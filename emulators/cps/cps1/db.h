#pragma once

#include "../../types.h"
#include <string>
#include <vector>

namespace cps1 {

// ROM entry types
enum class ROMType {
    PROGRAM,      // 68000 program ROM
    GRAPHICS,     // Graphics/tile ROM
    SOUND_PROGRAM,// Z80 sound program ROM
    SOUND_SAMPLE, // OKI6295 ADPCM samples
    PLD,          // PLD files (optional)
    UNKNOWN
};

// ROM entry flags
enum ROMFlags {
    ROM_FLAG_INTERLEAVE = 0x01,      // Program ROM needs interleaving
    ROM_FLAG_OPTIONAL = 0x02,      // Optional ROM (PLDs, etc.)
};

// ROM file entry
struct ROMEntry {
    const char* filename;
    u32 size;
    u32 crc32;
    ROMType type;
    u8 flags;
};

// CPS1 Board types (B-board revisions)
enum class CPSBoard {
    CPS_B_01 = 0,
    CPS_B_02 = 1,
    CPS_B_03 = 2,
    CPS_B_04 = 3,
    CPS_B_05 = 4,
    CPS_B_11 = 5,
    CPS_B_12 = 6,
    CPS_B_13 = 7,
    CPS_B_14 = 8,
    CPS_B_15 = 9,
    CPS_B_16 = 10,
    CPS_B_17 = 11,
    CPS_B_18 = 12,
    CPS_B_21_DEF = 13,
    CPS_B_21_BT1 = 14,
    CPS_B_21_BT2 = 15,
    CPS_B_21_BT3 = 16,
    CPS_B_21_BT4 = 17,
    CPS_B_21_BT5 = 18,
    CPS_B_21_BT6 = 19,
    CPS_B_21_BT7 = 20,
    CPS_B_21_QS1 = 21,
    CPS_B_21_QS2 = 22,
    CPS_B_21_QS3 = 23,
    CPS_B_21_QS4 = 24,
    CPS_B_21_QS5 = 25,
};

// Graphics ROM mappers
// Different boards organize graphics banks differently
enum class CPSMapper {
    MAPPER_LWCHR = 0,
    MAPPER_NM24B = 16,
    MAPPER_STF29 = 19,
    MAPPER_RT24B = 20,
    MAPPER_KD29B = 22,
    MAPPER_CC63B = 23,
    MAPPER_KR63B = 24,
    MAPPER_S9263B = 25,
    MAPPER_CP1B1F = 45,
    // Add more as needed for other games
};

// Board configuration
// Contains register locations and enable bits that vary per board
struct BoardConfig {
    u8 boardIdOffset;      // Offset where board ID is stored (from 0x800100)
    u8 boardIdValue1;      // First byte of board ID
    u8 boardIdValue2;      // Second byte of board ID
    u8 layerControlReg;    // Layer control register offset
    u8 paletteControlReg;  // Palette control register offset
    u8 maskAddr[4];        // Priority mask addresses
    u16 layerEnable[3];    // Layer enable bits (for layers 1-3)
};

// DIP switch information
// Maps a port address to its DIP switch value (stored inverted, hardware inverts on read)
struct DIPInfo {
    u16 port;              // Port address (e.g., 0x018, 0x019, 0x01A, etc.)
    u8 value;              // DIP switch value (stored inverted)
};

// Game database entry
struct GameInfo {
    const char* name;              // Game name
    const char* romSetName;        // MAME ROM set name
    const ROMEntry* roms;          // Array of ROM entries
    u32 romCount;                  // Number of ROM entries
    
    // Board configuration
    CPSBoard board;                // B-board type
    CPSMapper mapper;              // Graphics ROM mapper
    
    // DIP switch defaults
    // Array of port->value mappings for DIP switches
    // Values are stored inverted (hardware inverts on read)
    const DIPInfo* dipSwitches;    // Array of DIP switch entries
    u32 dipSwitchCount;            // Number of DIP switch entries
};

// Game database
class GameDatabase {
public:
    static const GameInfo* findGame(const std::string& romSetName);
    static bool validateROM(const std::string& filename, const std::vector<u8>& data, const ROMEntry& entry);
    static u32 calculateCRC32(const std::vector<u8>& data);
    
    // Get board configuration for a specific board type
    static BoardConfig getBoardConfig(CPSBoard board);
    
private:
    static const GameInfo s_games[];
    static const u32 s_gameCount;
};

} // namespace cps1
