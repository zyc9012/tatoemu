#include "db.h"
#include "../../../utilities/miniz/miniz.h"
#include <algorithm>
#include <cstring>

namespace cps1 {

// Street Fighter II: The World Warrior (sf2)
static const ROMEntry sf2_roms[] = {
    // Program ROMs (68000, byteswapped)
    { "sf2e_30g.11e",  0x020000, 0xfe39ee33, ROMType::PROGRAM, 0 },
    { "sf2e_37g.11f",  0x020000, 0xfb92cd74, ROMType::PROGRAM, 0 },
    { "sf2e_31g.12e",  0x020000, 0x69a0a301, ROMType::PROGRAM, 0 },
    { "sf2e_38g.12f",  0x020000, 0x5e22db70, ROMType::PROGRAM, 0 },
    { "sf2e_28g.9e",   0x020000, 0x8bf9f1e5, ROMType::PROGRAM, 0 },
    { "sf2e_35g.9f",   0x020000, 0x626ef934, ROMType::PROGRAM, 0 },
    { "sf2_29b.10e",   0x020000, 0xbb4af315, ROMType::PROGRAM, 0 },
    { "sf2_36b.10f",   0x020000, 0xc02a13eb, ROMType::PROGRAM, 0 },
    
    // Graphics ROMs
    { "sf2-5m.4a",     0x080000, 0x22c9cc8e, ROMType::GRAPHICS, 0 },
    { "sf2-7m.6a",     0x080000, 0x57213be8, ROMType::GRAPHICS, 0 },
    { "sf2-1m.3a",     0x080000, 0xba529b4f, ROMType::GRAPHICS, 0 },
    { "sf2-3m.5a",     0x080000, 0x4b1b33a8, ROMType::GRAPHICS, 0 },
    { "sf2-6m.4c",     0x080000, 0x2c7e2229, ROMType::GRAPHICS, 0 },
    { "sf2-8m.6c",     0x080000, 0xb5548f17, ROMType::GRAPHICS, 0 },
    { "sf2-2m.3c",     0x080000, 0x14b84312, ROMType::GRAPHICS, 0 },
    { "sf2-4m.5c",     0x080000, 0x5e9cd89a, ROMType::GRAPHICS, 0 },
    { "sf2-13m.4d",    0x080000, 0x994bfa58, ROMType::GRAPHICS, 0 },
    { "sf2-15m.6d",    0x080000, 0x3e66ad9d, ROMType::GRAPHICS, 0 },
    { "sf2-9m.3d",     0x080000, 0xc1befaa8, ROMType::GRAPHICS, 0 },
    { "sf2-11m.5d",    0x080000, 0x0627c831, ROMType::GRAPHICS, 0 },
    
    // Sound program ROM (Z80)
    { "sf2_9.12a",     0x010000, 0xa4823a1b, ROMType::SOUND_PROGRAM, 0 },
    
    // Sound samples (OKI6295)
    { "sf2_18.11c",    0x020000, 0x7f162009, ROMType::SOUND_SAMPLE, 0 },
    { "sf2_19.12c",    0x020000, 0xbeade53f, ROMType::SOUND_SAMPLE, 0 },
    
    // A-board PLDs (optional)
    { "buf1",          0x000117, 0xeb122de7, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "ioa1",          0x000117, 0x59c7ee3b, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "prg1",          0x000117, 0xf1129744, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "rom1",          0x000117, 0x41dc73b9, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "sou1",          0x000117, 0x84f4b2fe, ROMType::PLD, ROM_FLAG_OPTIONAL },
    
    // B-board PLDs (optional)
    { "stf29.1a",      0x000117, 0x043309c5, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "iob1.11d",      0x000117, 0x3abc0700, ROMType::PLD, ROM_FLAG_OPTIONAL },
    
    // C-board PLDs (optional)
    { "c632.ic1",      0x000117, 0x0fbd9270, ROMType::PLD, ROM_FLAG_OPTIONAL },
};

// Street Fighter II: Champion Edition (sf2ce)
static const ROMEntry sf2ce_roms[] = {
    // Program ROMs (68000, no byteswap)
    { "s92e_23b.8f",   0x080000, 0x0aaa1a3a, ROMType::PROGRAM, 0 },
    { "s92_22b.7f",    0x080000, 0x2bbe15ed, ROMType::PROGRAM, 0 },
    { "s92_21a.6f",    0x080000, 0x925a7877, ROMType::PROGRAM, 0 },
    
    // Graphics ROMs
    { "s92-1m.3a",     0x080000, 0x03b0d852, ROMType::GRAPHICS, 0 },
    { "s92-3m.5a",     0x080000, 0x840289ec, ROMType::GRAPHICS, 0 },
    { "s92-2m.4a",     0x080000, 0xcdb5f027, ROMType::GRAPHICS, 0 },
    { "s92-4m.6a",     0x080000, 0xe2799472, ROMType::GRAPHICS, 0 },
    { "s92-5m.7a",     0x080000, 0xba8a2761, ROMType::GRAPHICS, 0 },
    { "s92-7m.9a",     0x080000, 0xe584bfb5, ROMType::GRAPHICS, 0 },
    { "s92-6m.8a",     0x080000, 0x21e3f87d, ROMType::GRAPHICS, 0 },
    { "s92-8m.10a",    0x080000, 0xbefc47df, ROMType::GRAPHICS, 0 },
    { "s92-10m.3c",    0x080000, 0x960687d5, ROMType::GRAPHICS, 0 },
    { "s92-12m.5c",    0x080000, 0x978ecd18, ROMType::GRAPHICS, 0 },
    { "s92-11m.4c",    0x080000, 0xd6ec9a0a, ROMType::GRAPHICS, 0 },
    { "s92-13m.6c",    0x080000, 0xed2c67f6, ROMType::GRAPHICS, 0 },
    
    // Sound program ROM (Z80)
    { "s92_09.11a",    0x010000, 0x08f6b60e, ROMType::SOUND_PROGRAM, 0 },
    
    // Sound samples (OKI6295)
    { "s92_18.11c",    0x020000, 0x7f162009, ROMType::SOUND_SAMPLE, 0 },
    { "s92_19.12c",    0x020000, 0xbeade53f, ROMType::SOUND_SAMPLE, 0 },
    
    // A-board PLDs (optional)
    { "buf1",          0x000117, 0xeb122de7, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "ioa1",          0x000117, 0x59c7ee3b, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "prg1",          0x000117, 0xf1129744, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "rom1",          0x000117, 0x41dc73b9, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "sou1",          0x000117, 0x84f4b2fe, ROMType::PLD, ROM_FLAG_OPTIONAL },
    
    // B-board PLDs (optional)
    { "s9263b.1a",     0x000117, 0x0a7ecfe0, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "iob1.12d",      0x000117, 0x3abc0700, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "bprg1.11d",     0x000117, 0x31793da7, ROMType::PLD, ROM_FLAG_OPTIONAL },
    
    // C-board PLDs (optional)
    { "ioc1.ic7",      0x000104, 0xa399772d, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "c632.ic1",      0x000117, 0x0fbd9270, ROMType::PLD, ROM_FLAG_OPTIONAL },
};

// Game database
const GameInfo GameDatabase::s_games[] = {
    // Street Fighter II: The World Warrior
    // Config from FBNeo d_cps1.cpp line 17421: { "sf2", CPS_B_11, mapper_STF29, 0, NULL }
    // DIP switch defaults from FBNeo (d_cps1.cpp lines 4427-4429)
    {
        "Street Fighter II: The World Warrior",
        "sf2",
        sf2_roms,
        static_cast<u32>(sizeof(sf2_roms) / sizeof(sf2_roms[0])),
        true,                   // Program ROMs need byte swapping
        CPSBoard::CPS_B_11,     // B-board type
        CPSMapper::MAPPER_STF29,// Graphics ROM mapper
        0x00,                   // DIP 1: 1 coin 1 credit, upright cabinet
        0x03,                   // DIP 2: Medium difficulty
        0x60,                   // DIP 3: Demo sound ON, Allow continue ON, Free play OFF
        0xFF                    // DIP 4: Default
    },
    
    // Street Fighter II: Champion Edition
    // Config from FBNeo d_cps1.cpp line 17427: { "sf2ce", CPS_B_21_DEF, mapper_S9263B, 0, NULL }
    // DIP switch defaults from FBNeo (d_cps1.cpp lines 4707-4709)
    {
        "Street Fighter II: Champion Edition",
        "sf2ce",
        sf2ce_roms,
        static_cast<u32>(sizeof(sf2ce_roms) / sizeof(sf2ce_roms[0])),
        false,                     // Program ROMs don't need byte swapping
        CPSBoard::CPS_B_21_DEF,    // B-board type
        CPSMapper::MAPPER_S9263B,  // Graphics ROM mapper
        0x00,                      // DIP 1: 1 coin 1 credit, upright cabinet
        0x03,                      // DIP 2: Medium difficulty
        0x60,                      // DIP 3: Demo sound ON, Allow continue ON, Free play OFF
        0xFF                       // DIP 4: Default
    },
};

const u32 GameDatabase::s_gameCount = static_cast<u32>(sizeof(s_games) / sizeof(s_games[0]));

BoardConfig GameDatabase::getBoardConfig(CPSBoard board) {
    /*
     * Board Configuration Data
     * From FBNeo cps_config.cpp SetCpsBId() function
     * 
     * Each CPS1 B-board has different register layouts and board ID locations.
     * Games check the board ID to verify they're running on the right hardware.
     */
    
    BoardConfig config = {};
    
    switch (board) {
        case CPSBoard::CPS_B_11:
            // Street Fighter II: The World Warrior
            // From FBNeo cps_config.cpp lines 1171-1192
            config.boardIdOffset = 0x82;       // Board ID at 0x800182
            config.boardIdValue1 = 0x40;
            config.boardIdValue2 = 0x43;
            config.layerControlReg = 0x66;     // Layer control at 0x800166
            config.paletteControlReg = 0x70;   // Palette control at 0x800170
            config.maskAddr[0] = 0x68;
            config.maskAddr[1] = 0x6a;
            config.maskAddr[2] = 0x6c;
            config.maskAddr[3] = 0x6e;
            config.layerEnable[0] = 0x08;      // Layer 1 enable bit
            config.layerEnable[1] = 0x10;      // Layer 2 enable bit
            config.layerEnable[2] = 0x20;      // Layer 3 enable bit
            break;
            
        case CPSBoard::CPS_B_21_DEF:
            // Street Fighter II: Champion Edition and others
            // From FBNeo cps_config.cpp lines 1532-1557
            config.boardIdOffset = 0x32;       // Board ID at 0x800132
            config.boardIdValue1 = 0x00;
            config.boardIdValue2 = 0x00;
            config.layerControlReg = 0x66;     // Layer control at 0x800166
            config.paletteControlReg = 0x70;   // Palette control at 0x800170
            config.maskAddr[0] = 0x68;
            config.maskAddr[1] = 0x6a;
            config.maskAddr[2] = 0x6c;
            config.maskAddr[3] = 0x6e;
            config.layerEnable[0] = 0x02;      // Layer 1 enable bit
            config.layerEnable[1] = 0x04;      // Layer 2 enable bit
            config.layerEnable[2] = 0x08;      // Layer 3 enable bit
            break;
            
        default:
            // Fallback to CPS_B_11 defaults
            config.boardIdOffset = 0x82;
            config.boardIdValue1 = 0x40;
            config.boardIdValue2 = 0x43;
            config.layerControlReg = 0x66;
            config.paletteControlReg = 0x70;
            config.maskAddr[0] = 0x68;
            config.maskAddr[1] = 0x6a;
            config.maskAddr[2] = 0x6c;
            config.maskAddr[3] = 0x6e;
            config.layerEnable[0] = 0x08;      // Layer 1 enable bit
            config.layerEnable[1] = 0x10;      // Layer 2 enable bit
            config.layerEnable[2] = 0x20;      // Layer 3 enable bit
            break;
    }
    
    return config;
}

const GameInfo* GameDatabase::findGame(const std::string& romSetName) {
    // Convert to lowercase for case-insensitive matching
    std::string lower = romSetName;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    for (u32 i = 0; i < s_gameCount; i++) {
        std::string gameName = s_games[i].romSetName;
        std::transform(gameName.begin(), gameName.end(), gameName.begin(), ::tolower);
        
        if (lower == gameName) {
            return &s_games[i];
        }
    }
    
    return nullptr;
}

u32 GameDatabase::calculateCRC32(const std::vector<u8>& data) {
    return static_cast<u32>(mz_crc32(0, data.data(), static_cast<size_t>(data.size())));
}

bool GameDatabase::validateROM(const std::string& filename, const std::vector<u8>& data, const ROMEntry& entry) {
    // Check filename (case-insensitive)
    std::string lowerFilename = filename;
    std::transform(lowerFilename.begin(), lowerFilename.end(), lowerFilename.begin(), ::tolower);
    std::string lowerEntry = entry.filename;
    std::transform(lowerEntry.begin(), lowerEntry.end(), lowerEntry.begin(), ::tolower);
    
    if (lowerFilename != lowerEntry) {
        return false;
    }
    
    // Check size
    if (data.size() != entry.size) {
        return false;
    }
    
    // Check CRC32 (skip for optional ROMs)
    if (!(entry.flags & ROM_FLAG_OPTIONAL)) {
        u32 calculatedCRC = calculateCRC32(data);
        if (calculatedCRC != entry.crc32) {
            return false;
        }
    }
    
    return true;
}

} // namespace cps1
