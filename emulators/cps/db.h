#pragma once

#include "../types.h"
#include <string>
#include <vector>

namespace cps {

// Graphics types for ROM bank mapping (CPS1 only)
enum GfxType {
    GFXTYPE_SPRITES = (1 << 0),
    GFXTYPE_SCROLL1 = (1 << 1),
    GFXTYPE_SCROLL2 = (1 << 2),
    GFXTYPE_SCROLL3 = (1 << 3),
    GFXTYPE_STARS   = (1 << 4),
};

// ROM entry types (merged from CPS1 and CPS2)
enum class ROMType {
    PROGRAM,        // 68000 program ROM
    GRAPHICS,       // Graphics/tile ROM
    SOUND_PROGRAM,  // Z80 sound program ROM
    SOUND_SAMPLE,   // ADPCM sample ROMs (CPS1) or QSound samples (CPS2)
    PLD,            // PLD files (optional, CPS1 only)
    ENCRYPTION_KEY, // Decryption key (CPS2 only)
    UNKNOWN
};

// ROM entry flags (merged from CPS1 and CPS2)
enum ROMFlags {
    ROM_FLAG_INTERLEAVE = 0x01,  // Program ROM needs interleaving (CPS1 only)
    ROM_FLAG_OPTIONAL = 0x02,    // Optional ROM
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
    CPS_B_UNUSED = 255,
};

// Graphics bank range entry (for ROM bank mapping, CPS1 only)
struct GfxRange {
    u32 type;
    u32 start;
    u32 end;
    u32 bank;
};

// Graphics ROM mappers (CPS1 only)
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
    MAPPER_UNUSED = 255,
};

// Board configuration (CPS1 only)
struct BoardConfig {
    u8 boardIdOffset;      // Offset where board ID is stored (from 0x800100)
    u8 boardIdValue1;      // First byte of board ID
    u8 boardIdValue2;      // Second byte of board ID
    u8 layerControlReg;    // Layer control register offset
    u8 paletteControlReg;  // Palette control register offset
    u8 maskAddr[4];        // Priority mask addresses
    u16 layerEnable[3];    // Layer enable bits (for layers 1-3)
    u8 memProt[4];         // Memory protection offsets for multiplication registers
};

// Game database entry (unified for CPS1 and CPS2)
struct GameInfo {
    u8 cpsVer;                     // CPS version: 1 for CPS1, 2 for CPS2

    const char* name;              // Game name
    const char* romSetName;        // MAME ROM set name
    const ROMEntry* roms;          // Array of ROM entries
    u32 romCount;                  // Number of ROM entries
    
    // CPS1-specific fields
    CPSBoard board;                // B-board type
    CPSMapper mapper;              // Graphics ROM mapper
};

// Game database
class GameDatabase {
public:
    static const GameInfo* findGame(const std::string& romSetName);
    static bool validateROM(const std::string& filename, const std::vector<u8>& data, const ROMEntry& entry);
    static u32 calculateCRC32(const std::vector<u8>& data);
    
    // CPS1-specific functions
    static BoardConfig getBoardConfig(CPSBoard board);
    static const GfxRange* getGfxMapperTable(CPSMapper mapper);
    static void getGfxBankSizes(CPSMapper mapper, u32 sizes[4]);
    
private:
    static const GameInfo s_cps1_games[];
    static const GameInfo s_cps2_games[];
    static const u32 s_cps1_gameCount;
    static const u32 s_cps2_gameCount;
};

} // namespace cps
