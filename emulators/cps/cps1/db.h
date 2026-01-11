#pragma once

#include "../../types.h"
#include <string>
#include <vector>

namespace cps1 {

// Graphics types for ROM bank mapping
enum GfxType {
    GFXTYPE_SPRITES = (1 << 0),
    GFXTYPE_SCROLL1 = (1 << 1),
    GFXTYPE_SCROLL2 = (1 << 2),
    GFXTYPE_SCROLL3 = (1 << 3),
    GFXTYPE_STARS   = (1 << 4),
};

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

// Graphics bank range entry (for ROM bank mapping)
struct GfxRange {
    u32 type;
    u32 start;
    u32 end;
    u32 bank;
};

// Graphics ROM mappers
// Different boards organize graphics banks differently
enum class CPSMapper {
    MAPPER_LWCHR = 0,
    MAPPER_WL24B = 8,
    MAPPER_S224B = 9,
    MAPPER_YI24B = 10,
    MAPPER_O224B = 13,
    MAPPER_MS24B = 14,
    MAPPER_NM24B = 16,
    MAPPER_CA24B = 17,
    MAPPER_STF29 = 19,
    MAPPER_RT24B = 20,
    MAPPER_KD29B = 22,
    MAPPER_CC63B = 23,
    MAPPER_KR63B = 24,
    MAPPER_S9263B = 25,
    MAPPER_VA63B = 26,
    MAPPER_TK263B = 29,
    MAPPER_PS63B = 31,
    MAPPER_RCM63B = 36,
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
    u8 memProt[4];         // Memory protection offsets for multiplication registers
                           // [0] = write operand 1, [1] = write operand 2
                           // [2] = read low word, [3] = read high word
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
};

// Game database
class GameDatabase {
public:
    static const GameInfo* findGame(const std::string& romSetName);
    static bool validateROM(const std::string& filename, const std::vector<u8>& data, const ROMEntry& entry);
    static u32 calculateCRC32(const std::vector<u8>& data);
    
    // Get board configuration for a specific board type
    static BoardConfig getBoardConfig(CPSBoard board);
    
    // Get graphics ROM mapper table for a specific mapper
    static const GfxRange* getGfxMapperTable(CPSMapper mapper);
    
    // Get graphics ROM bank sizes for a specific mapper
    static void getGfxBankSizes(CPSMapper mapper, u32 sizes[4]);
    
private:
    static const GameInfo s_games[];
    static const u32 s_gameCount;
};

} // namespace cps1
