#include "db.h"
#include "../../../utilities/miniz/miniz.h"
#include <algorithm>
#include <cstring>

namespace cps1 {

// ============================================================================
// Graphics ROM Bank Mapper Tables
// ============================================================================

static const GfxRange mapper_STF29_table[] = {
    { GFXTYPE_SPRITES, 0x00000, 0x07fff, 0 },
    { GFXTYPE_SPRITES, 0x08000, 0x0ffff, 1 },
    { GFXTYPE_SPRITES, 0x10000, 0x11fff, 2 },
    { GFXTYPE_SCROLL3, 0x02000, 0x03fff, 2 },
    { GFXTYPE_SCROLL1, 0x04000, 0x04fff, 2 },
    { GFXTYPE_SCROLL2, 0x05000, 0x07fff, 2 },
    { 0, 0, 0, 0 }
};

static const GfxRange mapper_S9263B_table[] = {
    { GFXTYPE_SPRITES, 0x00000, 0x07fff, 0 },
    { GFXTYPE_SPRITES, 0x08000, 0x0ffff, 1 },
    { GFXTYPE_SPRITES, 0x10000, 0x11fff, 2 },
    { GFXTYPE_SCROLL3, 0x02000, 0x03fff, 2 },
    { GFXTYPE_SCROLL1, 0x04000, 0x04fff, 2 },
    { GFXTYPE_SCROLL2, 0x05000, 0x07fff, 2 },
    { 0, 0, 0, 0 }
};

static const GfxRange mapper_NM24B_table[] = {
    { GFXTYPE_SPRITES, 0x0000, 0x3fff, 0 },
    { GFXTYPE_SCROLL2, 0x0000, 0x3fff, 0 },
    { GFXTYPE_SCROLL1, 0x4000, 0x47ff, 0 },
    { GFXTYPE_SPRITES, 0x4800, 0x67ff, 0 },
    { GFXTYPE_SCROLL2, 0x4800, 0x67ff, 0 },
    { GFXTYPE_SCROLL3, 0x6800, 0x7fff, 0 },
    { 0, 0, 0, 0 }
};

static const GfxRange mapper_RT24B_table[] = {
    { GFXTYPE_SPRITES, 0x0000, 0x53ff, 0 },
    { GFXTYPE_SCROLL1, 0x5400, 0x6fff, 0 },
    { GFXTYPE_SCROLL3, 0x7000, 0x7fff, 0 },
    { GFXTYPE_SCROLL3, 0x0000, 0x3fff, 1 },
    { GFXTYPE_SCROLL2, 0x2800, 0x7fff, 1 },
    { GFXTYPE_SPRITES, 0x5400, 0x7fff, 1 },
    { 0, 0, 0, 0 }
};

static const GfxRange mapper_KD29B_table[] = {
    { GFXTYPE_SPRITES, 0x0000, 0x7fff, 0 },
    { GFXTYPE_SPRITES, 0x8000, 0x8fff, 1 },
    { GFXTYPE_SCROLL2, 0x9000, 0xbfff, 1 },
    { GFXTYPE_SCROLL1, 0xc000, 0xd7ff, 1 },
    { GFXTYPE_SCROLL3, 0xd800, 0xffff, 1 },
    { 0, 0, 0, 0 }
};

static const GfxRange mapper_CC63B_table[] = {
    { GFXTYPE_SPRITES, 0x0000, 0x7fff, 0 },
    { GFXTYPE_SCROLL2, 0x0000, 0x7fff, 0 },
    { GFXTYPE_SPRITES, 0x8000, 0xffff, 1 },
    { GFXTYPE_SCROLL1, 0x8000, 0xffff, 1 },
    { GFXTYPE_SCROLL2, 0x8000, 0xffff, 1 },
    { GFXTYPE_SCROLL3, 0x8000, 0xffff, 1 },
    { 0, 0, 0, 0 }
};

static const GfxRange mapper_KR63B_table[] = {
    { GFXTYPE_SPRITES, 0x0000, 0x7fff, 0 },
    { GFXTYPE_SCROLL2, 0x0000, 0x7fff, 0 },
    { GFXTYPE_SCROLL1, 0x8000, 0x9fff, 1 },
    { GFXTYPE_SPRITES, 0x8000, 0xcfff, 1 },
    { GFXTYPE_SCROLL2, 0x8000, 0xcfff, 1 },
    { GFXTYPE_SCROLL3, 0xd000, 0xffff, 1 },
    { 0, 0, 0, 0 }
};

static const GfxRange mapper_CP1B1F_table[] = {
    { GFXTYPE_SPRITES | GFXTYPE_SCROLL1 | GFXTYPE_SCROLL2 | GFXTYPE_SCROLL3, 0x0000, 0xffff, 0 },
    { 0, 0, 0, 0 }
};

static const GfxRange mapper_S224B_table[] = {
    { GFXTYPE_SPRITES, 0x0000, 0x43ff, 0 },
    { GFXTYPE_SCROLL1, 0x4400, 0x4bff, 0 },
    { GFXTYPE_SCROLL3, 0x4c00, 0x5fff, 0 },
    { GFXTYPE_SCROLL2, 0x6000, 0x7fff, 0 },
    { 0, 0, 0, 0 }
};

static const GfxRange mapper_TK263B_table[] = {
    { GFXTYPE_SPRITES | GFXTYPE_SCROLL1 | GFXTYPE_SCROLL2 | GFXTYPE_SCROLL3, 0x00000, 0x07fff, 0 },
    { GFXTYPE_SPRITES | GFXTYPE_SCROLL1 | GFXTYPE_SCROLL2 | GFXTYPE_SCROLL3, 0x08000, 0x0ffff, 1 },
    { 0, 0, 0, 0 }
};

static const GfxRange mapper_PS63B_table[] = {
    { GFXTYPE_SCROLL1, 0x0000, 0x0fff, 0 },
    { GFXTYPE_SPRITES, 0x1000, 0x7fff, 0 },
    { GFXTYPE_SPRITES | GFXTYPE_SCROLL2, 0x8000, 0xdbff, 1 },
    { GFXTYPE_SCROLL3, 0xdc00, 0xffff, 1 },
    { 0, 0, 0, 0 }
};

static const GfxRange mapper_RCM63B_table[] = {
    { GFXTYPE_SPRITES | GFXTYPE_SCROLL1 | GFXTYPE_SCROLL2 | GFXTYPE_SCROLL3, 0x00000, 0x07fff, 0 },
    { GFXTYPE_SPRITES | GFXTYPE_SCROLL1 | GFXTYPE_SCROLL2 | GFXTYPE_SCROLL3, 0x08000, 0x0ffff, 1 },
    { GFXTYPE_SPRITES | GFXTYPE_SCROLL1 | GFXTYPE_SCROLL2 | GFXTYPE_SCROLL3, 0x10000, 0x17fff, 2 },
    { GFXTYPE_SPRITES | GFXTYPE_SCROLL1 | GFXTYPE_SCROLL2 | GFXTYPE_SCROLL3, 0x18000, 0x1ffff, 3 },
    { 0, 0, 0, 0 }
};

static const GfxRange mapper_WL24B_table[] = {
    { GFXTYPE_SPRITES, 0x0000, 0x4fff, 0 },
    { GFXTYPE_SCROLL3, 0x5000, 0x6fff, 0 },
    { GFXTYPE_SCROLL1, 0x7000, 0x7fff, 0 },
    { GFXTYPE_SCROLL2, 0x0000, 0x3fff, 1 },
    { 0, 0, 0, 0 }
};

static const GfxRange mapper_O224B_table[] = {
    { GFXTYPE_SCROLL1, 0x0000, 0x0bff, 0 },
    { GFXTYPE_SCROLL2, 0x0c00, 0x3bff, 0 },
    { GFXTYPE_SCROLL3, 0x3c00, 0x4bff, 0 },
    { GFXTYPE_SPRITES, 0x4c00, 0x7fff, 0 },
    { GFXTYPE_SPRITES, 0x8000, 0xa7ff, 1 },
    { GFXTYPE_SCROLL2, 0xa800, 0xb7ff, 1 },
    { GFXTYPE_SCROLL3, 0xb800, 0xbfff, 1 },
    { 0, 0, 0, 0 }
};

static const GfxRange mapper_VA63B_table[] = {
    { GFXTYPE_SPRITES | GFXTYPE_SCROLL1 | GFXTYPE_SCROLL2 | GFXTYPE_SCROLL3, 0x00000, 0x07fff, 0 },
    { 0, 0, 0, 0 }
};

static const GfxRange mapper_CA24B_table[] = {
    { GFXTYPE_SPRITES, 0x0000, 0x2fff, 0 },
    { GFXTYPE_SCROLL2, 0x0000, 0x2fff, 0 },
    { GFXTYPE_SCROLL3, 0x3000, 0x4fff, 0 },
    { GFXTYPE_SCROLL1, 0x5000, 0x57ff, 0 },
    { GFXTYPE_SPRITES, 0x5800, 0x7fff, 0 },
    { GFXTYPE_SCROLL2, 0x5800, 0x7fff, 0 },
    { 0, 0, 0, 0 }
};

static const GfxRange mapper_YI24B_table[] = {
    { GFXTYPE_SPRITES, 0x0000, 0x1fff, 0 },
    { GFXTYPE_SCROLL3, 0x2000, 0x3fff, 0 },
    { GFXTYPE_SCROLL1, 0x4000, 0x47ff, 0 },
    { GFXTYPE_SCROLL2, 0x4800, 0x7fff, 0 },
    { 0, 0, 0, 0 }
};

static const GfxRange mapper_MS24B_table[] = {
    { GFXTYPE_SPRITES, 0x0000, 0x3fff, 0 },
    { GFXTYPE_SCROLL1, 0x4000, 0x4fff, 0 },
    { GFXTYPE_SCROLL2, 0x5000, 0x6fff, 0 },
    { GFXTYPE_SCROLL3, 0x7000, 0x7fff, 0 },
    { 0, 0, 0, 0 }
};

// Street Fighter II: The World Warrior (sf2)
static const ROMEntry sf2_roms[] = {
    // Program ROMs (68000)
    { "sf2e_30g.11e",  0x020000, 0xfe39ee33, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "sf2e_37g.11f",  0x020000, 0xfb92cd74, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "sf2e_31g.12e",  0x020000, 0x69a0a301, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "sf2e_38g.12f",  0x020000, 0x5e22db70, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "sf2e_28g.9e",   0x020000, 0x8bf9f1e5, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "sf2e_35g.9f",   0x020000, 0x626ef934, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "sf2_29b.10e",   0x020000, 0xbb4af315, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "sf2_36b.10f",   0x020000, 0xc02a13eb, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    
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
    // Program ROMs (68000)
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

// Street Fighter II: Hyper Fighting (sf2hf)
static const ROMEntry sf2hf_roms[] = {
    // Program ROMs (68000)
    { "s2te_23.8f",    0x080000, 0x2dd72514, ROMType::PROGRAM, 0 },
    { "s2te_22.7f",    0x080000, 0xaea6e035, ROMType::PROGRAM, 0 },
    { "s2te_21.6f",    0x080000, 0xfd200288, ROMType::PROGRAM, 0 },

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

// Three Wonders (3wonders)
static const ROMEntry threewonders_roms[] = {
    // Program ROMs (68000)
    { "rte_30a.11f",   0x020000, 0xef5b8b33, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "rte_35a.11h",   0x020000, 0x7d705529, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "rte_31a.12f",   0x020000, 0x32835e5e, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "rte_36a.12h",   0x020000, 0x7637975f, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "rt_28a.9f",     0x020000, 0x054137c8, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "rt_33a.9h",     0x020000, 0x7264cb1b, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "rte_29a.10f",   0x020000, 0xcddaa919, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "rte_34a.10h",   0x020000, 0xed52e7e5, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    
    // Graphics ROMs
    { "rt-5m.7a",      0x080000, 0x86aef804, ROMType::GRAPHICS, 0 },
    { "rt-7m.9a",      0x080000, 0x4f057110, ROMType::GRAPHICS, 0 },
    { "rt-1m.3a",      0x080000, 0x902489d0, ROMType::GRAPHICS, 0 },
    { "rt-3m.5a",      0x080000, 0xe35ce720, ROMType::GRAPHICS, 0 },
    { "rt-6m.8a",      0x080000, 0x13cb0e7c, ROMType::GRAPHICS, 0 },
    { "rt-8m.10a",     0x080000, 0x1f055014, ROMType::GRAPHICS, 0 },
    { "rt-2m.4a",      0x080000, 0xe9a034f4, ROMType::GRAPHICS, 0 },
    { "rt-4m.6a",      0x080000, 0xdf0eea8b, ROMType::GRAPHICS, 0 },
    
    // Sound program ROM (Z80)
    { "rt_9.12b",      0x010000, 0xabfca165, ROMType::SOUND_PROGRAM, 0 },
    
    // Sound samples (OKI6295)
    { "rt_18.11c",     0x020000, 0x26b211ab, ROMType::SOUND_SAMPLE, 0 },
    { "rt_19.12c",     0x020000, 0xdbe64ad0, ROMType::SOUND_SAMPLE, 0 },
    
    // A-board PLDs (optional)
    { "buf1",          0x000117, 0xeb122de7, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "ioa1",          0x000117, 0x59c7ee3b, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "prg1",          0x000117, 0xf1129744, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "rom1",          0x000117, 0x41dc73b9, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "sou1",          0x000117, 0x84f4b2fe, ROMType::PLD, ROM_FLAG_OPTIONAL },
    
    // B-board PLDs (optional)
    { "rt24b.1a",      0x000117, 0x54b85159, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "iob1.11e",      0x000117, 0x3abc0700, ROMType::PLD, ROM_FLAG_OPTIONAL },
    
    // C-board PLDs (optional)
    { "ioc1.ic1",      0x000104, 0xa399772d, ROMType::PLD, ROM_FLAG_OPTIONAL },
};

// Captain Commando (captcomm)
static const ROMEntry captcomm_roms[] = {
    // Program ROMs (68000)
    { "cce_23f.8f",    0x080000, 0x42c814c5, ROMType::PROGRAM, 0 },
    { "cc_22f.7f",     0x080000, 0x0fd34195, ROMType::PROGRAM, 0 },
    { "cc_24f.9e",     0x020000, 0x3a794f25, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "cc_28f.9f",     0x020000, 0xfc3c2906, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    
    // Graphics ROMs
    { "cc-5m.3a",      0x080000, 0x7261d8ba, ROMType::GRAPHICS, 0 },
    { "cc-7m.5a",      0x080000, 0x6a60f949, ROMType::GRAPHICS, 0 },
    { "cc-1m.4a",      0x080000, 0x00637302, ROMType::GRAPHICS, 0 },
    { "cc-3m.6a",      0x080000, 0xcc87cf61, ROMType::GRAPHICS, 0 },
    { "cc-6m.7a",      0x080000, 0x28718bed, ROMType::GRAPHICS, 0 },
    { "cc-8m.9a",      0x080000, 0xd4acc53a, ROMType::GRAPHICS, 0 },
    { "cc-2m.8a",      0x080000, 0x0c69f151, ROMType::GRAPHICS, 0 },
    { "cc-4m.10a",     0x080000, 0x1f9ebb97, ROMType::GRAPHICS, 0 },
    
    // Sound program ROM (Z80)
    { "cc_09.11a",     0x010000, 0x698e8b58, ROMType::SOUND_PROGRAM, 0 },
    
    // Sound samples (OKI6295)
    { "cc_18.11c",     0x020000, 0x6de2c2db, ROMType::SOUND_SAMPLE, 0 },
    { "cc_19.12c",     0x020000, 0xb99091ae, ROMType::SOUND_SAMPLE, 0 },
    
    // A-board PLDs (optional)
    { "buf1",          0x000117, 0xeb122de7, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "ioa1",          0x000117, 0x59c7ee3b, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "prg1",          0x000117, 0xf1129744, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "rom1",          0x000117, 0x41dc73b9, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "sou1",          0x000117, 0x84f4b2fe, ROMType::PLD, ROM_FLAG_OPTIONAL },
    
    // B-board PLDs (optional)
    { "cc63b.1a",      0x000117, 0xcae8f0f9, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "iob1.12d",      0x000117, 0x3abc0700, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "ccprg1.11d",    0x000117, 0xe1c225c4, ROMType::PLD, ROM_FLAG_OPTIONAL },
    
    // C-board PLDs (optional)
    { "ioc1.ic7",      0x000104, 0xa399772d, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "c632.ic1",      0x000117, 0x0fbd9270, ROMType::PLD, ROM_FLAG_OPTIONAL },
};

// Knights of the Round (knights)
static const ROMEntry knights_roms[] = {
    // Program ROMs (68000)
    { "kr_23e.8f",     0x080000, 0x1b3997eb, ROMType::PROGRAM, 0 },
    { "kr_22.7f",      0x080000, 0xd0b671a9, ROMType::PROGRAM, 0 },
    
    // Graphics ROMs
    { "kr-5m.3a",      0x080000, 0x9e36c1a4, ROMType::GRAPHICS, 0 },
    { "kr-7m.5a",      0x080000, 0xc5832cae, ROMType::GRAPHICS, 0 },
    { "kr-1m.4a",      0x080000, 0xf095be2d, ROMType::GRAPHICS, 0 },
    { "kr-3m.6a",      0x080000, 0x179dfd96, ROMType::GRAPHICS, 0 },
    { "kr-6m.7a",      0x080000, 0x1f4298d2, ROMType::GRAPHICS, 0 },
    { "kr-8m.9a",      0x080000, 0x37fa8751, ROMType::GRAPHICS, 0 },
    { "kr-2m.8a",      0x080000, 0x0200bc3d, ROMType::GRAPHICS, 0 },
    { "kr-4m.10a",     0x080000, 0x0bb2b4e7, ROMType::GRAPHICS, 0 },
    
    // Sound program ROM (Z80)
    { "kr_09.11a",     0x010000, 0x5e44d9ee, ROMType::SOUND_PROGRAM, 0 },
    
    // Sound samples (OKI6295)
    { "kr_18.11c",     0x020000, 0xda69d15f, ROMType::SOUND_SAMPLE, 0 },
    { "kr_19.12c",     0x020000, 0xbfc654e9, ROMType::SOUND_SAMPLE, 0 },
    
    // A-board PLDs (optional)
    { "buf1",          0x000117, 0xeb122de7, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "ioa1",          0x000117, 0x59c7ee3b, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "prg1",          0x000117, 0xf1129744, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "rom1",          0x000117, 0x41dc73b9, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "sou1",          0x000117, 0x84f4b2fe, ROMType::PLD, ROM_FLAG_OPTIONAL },
    
    // B-board PLDs (optional)
    { "kr63b.1a",      0x000117, 0xfd5b6522, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "iob1.12d",      0x000117, 0x3abc0700, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "bprg1.11d",     0x000117, 0x31793da7, ROMType::PLD, ROM_FLAG_OPTIONAL },
    
    // C-board PLDs (optional)
    { "ioc1.ic7",      0x000104, 0xa399772d, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "c632.ic1",      0x000117, 0x0fbd9270, ROMType::PLD, ROM_FLAG_OPTIONAL },
};

// The King of Dragons (kod)
static const ROMEntry kod_roms[] = {
    // Program ROMs (68000)
    { "kde_30a.11e",   0x020000, 0xfcb5efe2, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "kde_37a.11f",   0x020000, 0xf22e5266, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "kde_31a.12e",   0x020000, 0xc710d722, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "kde_38a.12f",   0x020000, 0x57d6ed3a, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "kd_28.9e",      0x020000, 0x9367bcd9, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "kd_35.9f",      0x020000, 0x4ca6a48a, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "kd_29.10e",     0x020000, 0x0360fa72, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "kd_36a.10f",    0x020000, 0x95a3cef8, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    
    // Graphics ROMs
    { "kd-5m.4a",      0x080000, 0xe45b8701, ROMType::GRAPHICS, 0 },
    { "kd-7m.6a",      0x080000, 0xa7750322, ROMType::GRAPHICS, 0 },
    { "kd-1m.3a",      0x080000, 0x5f74bf78, ROMType::GRAPHICS, 0 },
    { "kd-3m.5a",      0x080000, 0x5e5303bf, ROMType::GRAPHICS, 0 },
    { "kd-6m.4c",      0x080000, 0x113358f3, ROMType::GRAPHICS, 0 },
    { "kd-8m.6c",      0x080000, 0x38853c44, ROMType::GRAPHICS, 0 },
    { "kd-2m.3c",      0x080000, 0x9ef36604, ROMType::GRAPHICS, 0 },
    { "kd-4m.5c",      0x080000, 0x402b9b4f, ROMType::GRAPHICS, 0 },
    
    // Sound program ROM (Z80)
    { "kd_9.12a",      0x010000, 0xbac6ec26, ROMType::SOUND_PROGRAM, 0 },
    
    // Sound samples (OKI6295)
    { "kd_18.11c",     0x020000, 0x4c63181d, ROMType::SOUND_SAMPLE, 0 },
    { "kd_19.12c",     0x020000, 0x92941b80, ROMType::SOUND_SAMPLE, 0 },
    
    // A-board PLDs (optional)
    { "buf1",          0x000117, 0xeb122de7, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "ioa1",          0x000117, 0x59c7ee3b, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "prg1",          0x000117, 0xf1129744, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "rom1",          0x000117, 0x41dc73b9, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "sou1",          0x000117, 0x84f4b2fe, ROMType::PLD, ROM_FLAG_OPTIONAL },
    
    // B-board PLDs (optional)
    { "kd29b.1a",      0x000117, 0x6b892f82, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "iob1.11d",      0x000117, 0x3abc0700, ROMType::PLD, ROM_FLAG_OPTIONAL },
    
    // C-board PLDs (optional)
    { "ioc1.ic7",      0x000104, 0xa399772d, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "c632.ic1",      0x000117, 0x0fbd9270, ROMType::PLD, ROM_FLAG_OPTIONAL },
};

// Nemo (nemo)
static const ROMEntry nemo_roms[] = {
    // Program ROMs (68000)
    { "nme_30a.11f",   0x020000, 0xd2c03e56, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "nme_35a.11h",   0x020000, 0x5fd31661, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "nme_31a.12f",   0x020000, 0xb2bd4f6f, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "nme_36a.12h",   0x020000, 0xee9450e3, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "nm-32m.8h",     0x080000, 0xd6d1add3, ROMType::PROGRAM, 0 },
    
    // Graphics ROMs
    { "nm-5m.7a",      0x080000, 0x487b8747, ROMType::GRAPHICS, 0 },
    { "nm-7m.9a",      0x080000, 0x203dc8c6, ROMType::GRAPHICS, 0 },
    { "nm-1m.3a",      0x080000, 0x9e878024, ROMType::GRAPHICS, 0 },
    { "nm-3m.5a",      0x080000, 0xbb01e6b6, ROMType::GRAPHICS, 0 },
    
    // Sound program ROM (Z80)
    { "nme_09.12b",    0x010000, 0x0f4b0581, ROMType::SOUND_PROGRAM, 0 },
    
    // Sound samples (OKI6295)
    { "nme_18.11c",    0x020000, 0xbab333d4, ROMType::SOUND_SAMPLE, 0 },
    { "nme_19.12c",    0x020000, 0x2650a0a8, ROMType::SOUND_SAMPLE, 0 },
    
    // A-board PLDs (optional)
    { "buf1",          0x000117, 0xeb122de7, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "ioa1",          0x000117, 0x59c7ee3b, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "prg1",          0x000117, 0xf1129744, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "rom1",          0x000117, 0x41dc73b9, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "sou1",          0x000117, 0x84f4b2fe, ROMType::PLD, ROM_FLAG_OPTIONAL },
    
    // B-board PLDs (optional)
    { "nm24b.1a",      0x000117, 0x7b25bac6, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "iob1.11e",      0x000117, 0x3abc0700, ROMType::PLD, ROM_FLAG_OPTIONAL },
    
    // C-board PLDs (optional)
    { "ioc1.ic1",      0x000104, 0xa399772d, ROMType::PLD, ROM_FLAG_OPTIONAL },
};

// Final Fight (ffight)
static const ROMEntry ffight_roms[] = {
    // Program ROMs (68000)
    { "ff_36.11f",     0x020000, 0xf9a5ce83, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "ff_42.11h",     0x020000, 0x65f11215, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "ff_37.12f",     0x020000, 0xe1033784, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "ffe_43.12h",    0x020000, 0x995e968a, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "ff-32m.8h",     0x080000, 0xc747696e, ROMType::PROGRAM, 0 },
    
    // Graphics ROMs
    { "ff-5m.7a",      0x080000, 0x9c284108, ROMType::GRAPHICS, 0 },
    { "ff-7m.9a",      0x080000, 0xa7584dfb, ROMType::GRAPHICS, 0 },
    { "ff-1m.3a",      0x080000, 0x0b605e44, ROMType::GRAPHICS, 0 },
    { "ff-3m.5a",      0x080000, 0x52291cd2, ROMType::GRAPHICS, 0 },
    
    // Sound program ROM (Z80)
    { "ff_09.12b",     0x010000, 0xb8367eb5, ROMType::SOUND_PROGRAM, 0 },
    
    // Sound samples (OKI6295)
    { "ff_18.11c",     0x020000, 0x375c66e7, ROMType::SOUND_SAMPLE, 0 },
    { "ff_19.12c",     0x020000, 0x1ef137f9, ROMType::SOUND_SAMPLE, 0 },
    
    // A-board PLDs (optional)
    { "buf1",          0x000117, 0xeb122de7, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "ioa1",          0x000117, 0x59c7ee3b, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "prg1",          0x000117, 0xf1129744, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "rom1",          0x000117, 0x41dc73b9, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "sou1",          0x000117, 0x84f4b2fe, ROMType::PLD, ROM_FLAG_OPTIONAL },
    
    // B-board PLDs (optional)
    { "s224b.1a",      0x000117, 0xcdc4413e, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "iob1.11e",      0x000117, 0x3abc0700, ROMType::PLD, ROM_FLAG_OPTIONAL },
};

// Warriors of Fate / Tenchi wo Kurau II (wof)
static const ROMEntry wof_roms[] = {
    // Program ROMs (68000)
    { "tk2e_23c.8f",   0x080000, 0x0d708505, ROMType::PROGRAM, 0 },
    { "tk2e_22c.7f",   0x080000, 0x608c17e3, ROMType::PROGRAM, 0 },
    
    // Graphics ROMs
    { "tk2-1m.3a",     0x080000, 0x0d9cb9bf, ROMType::GRAPHICS, 0 },
    { "tk2-3m.5a",     0x080000, 0x45227027, ROMType::GRAPHICS, 0 },
    { "tk2-2m.4a",     0x080000, 0xc5ca2460, ROMType::GRAPHICS, 0 },
    { "tk2-4m.6a",     0x080000, 0xe349551c, ROMType::GRAPHICS, 0 },
    { "tk2-5m.7a",     0x080000, 0x291f0f0b, ROMType::GRAPHICS, 0 },
    { "tk2-7m.9a",     0x080000, 0x3edeb949, ROMType::GRAPHICS, 0 },
    { "tk2-6m.8a",     0x080000, 0x1abd14d6, ROMType::GRAPHICS, 0 },
    { "tk2-8m.10a",    0x080000, 0xb27948e3, ROMType::GRAPHICS, 0 },
    
    // Sound program ROM (Z80)
    { "tk2_qa.5k",     0x020000, 0xc9183a0d, ROMType::SOUND_PROGRAM, 0 },
    
    // QSound samples
    { "tk2-q1.1k",     0x080000, 0x611268cf, ROMType::SOUND_SAMPLE, 0 },
    { "tk2-q2.2k",     0x080000, 0x20f55ca9, ROMType::SOUND_SAMPLE, 0 },
    { "tk2-q3.3k",     0x080000, 0xbfcf6f52, ROMType::SOUND_SAMPLE, 0 },
    { "tk2-q4.4k",     0x080000, 0x36642e88, ROMType::SOUND_SAMPLE, 0 },
    
    // A-board PLDs (optional)
    { "buf1",          0x000117, 0xeb122de7, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "ioa1",          0x000117, 0x59c7ee3b, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "prg2",          0x000117, 0x4386879a, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "rom1",          0x000117, 0x41dc73b9, ROMType::PLD, ROM_FLAG_OPTIONAL },
    
    // B-board PLDs (optional)
    { "tk263b.1a",     0x000117, 0xc4b0349b, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "iob1.12d",      0x000117, 0x3abc0700, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "bprg1.11d",     0x000117, 0x31793da7, ROMType::PLD, ROM_FLAG_OPTIONAL },
    
    // C-board PLDs (optional)
    { "ioc1.ic1",      0x000104, 0xa399772d, ROMType::PLD, ROM_FLAG_OPTIONAL },

    // D-board PLDs (optional)
    { "d7l1.7l",       0x000117, 0x27b7410d, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "d8l1.8l",       0x000117, 0x539fc7da, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "d9k1.9k",       0x000117, 0x6c35c805, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "d10f1.10f",     0x000117, 0x6619c494, ROMType::PLD, ROM_FLAG_OPTIONAL },
};

// The Punisher (punisher)
static const ROMEntry punisher_roms[] = {
    // Program ROMs (68000)
    { "pse_26.11e",    0x020000, 0x389a99d2, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "pse_30.11f",    0x020000, 0x68fb06ac, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "pse_27.12e",    0x020000, 0x3eb181c3, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "pse_31.12f",    0x020000, 0x37108e7b, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "pse_24.9e",     0x020000, 0x0f434414, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "pse_28.9f",     0x020000, 0xb732345d, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "pse_25.10e",    0x020000, 0xb77102e2, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "pse_29.10f",    0x020000, 0xec037bce, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "ps_21.6f",      0x080000, 0x8affa5a9, ROMType::PROGRAM, 0 },
    
    // Graphics ROMs
    { "ps-1m.3a",      0x080000, 0x77b7ccab, ROMType::GRAPHICS, 0 },
    { "ps-3m.5a",      0x080000, 0x0122720b, ROMType::GRAPHICS, 0 },
    { "ps-2m.4a",      0x080000, 0x64fa58d4, ROMType::GRAPHICS, 0 },
    { "ps-4m.6a",      0x080000, 0x60da42c8, ROMType::GRAPHICS, 0 },
    { "ps-5m.7a",      0x080000, 0xc54ea839, ROMType::GRAPHICS, 0 },
    { "ps-7m.9a",      0x080000, 0x04c5acbd, ROMType::GRAPHICS, 0 },
    { "ps-6m.8a",      0x080000, 0xa544f4cc, ROMType::GRAPHICS, 0 },
    { "ps-8m.10a",     0x080000, 0x8f02f436, ROMType::GRAPHICS, 0 },
    
    // Sound program ROM (Z80)
    { "ps_q.5k",       0x020000, 0x49ff4446, ROMType::SOUND_PROGRAM, 0 },
    
    // QSound samples
    { "ps-q1.1k",      0x080000, 0x31fd8726, ROMType::SOUND_SAMPLE, 0 },
    { "ps-q2.2k",      0x080000, 0x980a9eef, ROMType::SOUND_SAMPLE, 0 },
    { "ps-q3.3k",      0x080000, 0x0dd44491, ROMType::SOUND_SAMPLE, 0 },
    { "ps-q4.4k",      0x080000, 0xbed42f03, ROMType::SOUND_SAMPLE, 0 },
    
    // A-board PLDs (optional)
    { "buf1",          0x000117, 0xeb122de7, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "ioa1",          0x000117, 0x59c7ee3b, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "prg2",          0x000117, 0x4386879a, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "rom1",          0x000117, 0x41dc73b9, ROMType::PLD, ROM_FLAG_OPTIONAL },
    
    // B-board PLDs (optional)
    { "ps63b.1a",      0x000117, 0x03a758b0, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "iob1.12d",      0x000117, 0x3abc0700, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "bprg1.11d",     0x000117, 0x31793da7, ROMType::PLD, ROM_FLAG_OPTIONAL },
    
    // C-board PLDs (optional)
    { "ioc1.ic1",      0x000104, 0xa399772d, ROMType::PLD, ROM_FLAG_OPTIONAL },

    // D-board PLDs (optional)
    { "d7l1.7l",       0x000117, 0x27b7410d, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "d8l1.8l",       0x000117, 0x539fc7da, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "d9k2.9k",       0x000117, 0xcd85a156, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "d10f1.10f",     0x000117, 0x6619c494, ROMType::PLD, ROM_FLAG_OPTIONAL },
};

// Mega Man: The Power Battle (megaman)
static const ROMEntry megaman_roms[] = {
    // Program ROMs (68000)
    { "rcmu_23b.8f",   0x080000, 0x1cd33c7a, ROMType::PROGRAM, 0 },
    { "rcmu_22b.7f",   0x080000, 0x708268c4, ROMType::PROGRAM, 0 },
    { "rcmu_21a.6f",   0x080000, 0x4376ea95, ROMType::PROGRAM, 0 },
    
    // Graphics ROMs
    { "rcm_01.3a",     0x080000, 0x6ecdf13f, ROMType::GRAPHICS, 0 },
    { "rcm_02.4a",     0x080000, 0x944d4f0f, ROMType::GRAPHICS, 0 },
    { "rcm_03.5a",     0x080000, 0x36f3073c, ROMType::GRAPHICS, 0 },
    { "rcm_04.6a",     0x080000, 0x54e622ff, ROMType::GRAPHICS, 0 },
    { "rcm_05.7a",     0x080000, 0x5dd131fd, ROMType::GRAPHICS, 0 },
    { "rcm_06.8a",     0x080000, 0xf0faf813, ROMType::GRAPHICS, 0 },
    { "rcm_07.9a",     0x080000, 0x826de013, ROMType::GRAPHICS, 0 },
    { "rcm_08.10a",    0x080000, 0xfbff64cf, ROMType::GRAPHICS, 0 },
    { "rcm_10.3c",     0x080000, 0x4dc8ada9, ROMType::GRAPHICS, 0 },
    { "rcm_11.4c",     0x080000, 0xf2b9ee06, ROMType::GRAPHICS, 0 },
    { "rcm_12.5c",     0x080000, 0xfed5f203, ROMType::GRAPHICS, 0 },
    { "rcm_13.6c",     0x080000, 0x5069d4a9, ROMType::GRAPHICS, 0 },
    { "rcm_14.7c",     0x080000, 0x303be3bd, ROMType::GRAPHICS, 0 },
    { "rcm_15.8c",     0x080000, 0x4f2d372f, ROMType::GRAPHICS, 0 },
    { "rcm_16.9c",     0x080000, 0x93d97fde, ROMType::GRAPHICS, 0 },
    { "rcm_17.10c",    0x080000, 0x92371042, ROMType::GRAPHICS, 0 },
    
    // Sound program ROM (Z80)
    { "rcm_09.11a",    0x010000, 0x22ac8f5f, ROMType::SOUND_PROGRAM, 0 },
    
    // Sound samples (OKI6295)
    { "rcm_18.11c",    0x020000, 0x80f1f8aa, ROMType::SOUND_SAMPLE, 0 },
    { "rcm_19.12c",    0x020000, 0xf257dbe1, ROMType::SOUND_SAMPLE, 0 },
    
    // A-board PLDs (optional)
    { "buf1",          0x000117, 0xeb122de7, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "ioa1",          0x000117, 0x59c7ee3b, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "prg1",          0x000117, 0xf1129744, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "rom1",          0x000117, 0x41dc73b9, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "sou1",          0x000117, 0x84f4b2fe, ROMType::PLD, ROM_FLAG_OPTIONAL },
    
    // B-board PLDs (optional)
    { "rcm63b.1a",     0x000117, 0x84acd494, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "iob1.12d",      0x000117, 0x3abc0700, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "bprg1.11d",     0x000117, 0x31793da7, ROMType::PLD, ROM_FLAG_OPTIONAL },
    
    // C-board PLDs (optional)
    { "ioc1.ic7",      0x000104, 0xa399772d, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "c632.ic1",      0x000117, 0x0fbd9270, ROMType::PLD, ROM_FLAG_OPTIONAL },
};

// Willow (willow)
static const ROMEntry willow_roms[] = {
    // Program ROMs (68000)
    { "wle_30.11f",    0x020000, 0x15372aa2, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "wle_35.11h",    0x020000, 0x2e64623b, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "wlu_31.12f",    0x020000, 0x0eb48a83, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "wlu_36.12h",    0x020000, 0x36100209, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "wlm-32.8h",     0x080000, 0xdfd9f643, ROMType::PROGRAM, 0 },
    
    // Graphics ROMs
    { "wlm-7.7a",      0x080000, 0xafa74b73, ROMType::GRAPHICS, 0 },
    { "wlm-5.9a",      0x080000, 0x12a0dc0b, ROMType::GRAPHICS, 0 },
    { "wlm-3.3a",      0x080000, 0xc6f2abce, ROMType::GRAPHICS, 0 },
    { "wlm-1.5a",      0x080000, 0x4aa4c6d3, ROMType::GRAPHICS, 0 },
    { "wl_24.7d",      0x020000, 0x6f0adee5, ROMType::GRAPHICS, 0 },
    { "wl_14.7c",      0x020000, 0x9cf3027d, ROMType::GRAPHICS, 0 },
    { "wl_26.9d",      0x020000, 0xf09c8ecf, ROMType::GRAPHICS, 0 },
    { "wl_16.9c",      0x020000, 0xe35407aa, ROMType::GRAPHICS, 0 },
    { "wl_20.3d",      0x020000, 0x84992350, ROMType::GRAPHICS, 0 },
    { "wl_10.3c",      0x020000, 0xb87b5a36, ROMType::GRAPHICS, 0 },
    { "wl_22.5d",      0x020000, 0xfd3f89f0, ROMType::GRAPHICS, 0 },
    { "wl_12.5c",      0x020000, 0x7da49d69, ROMType::GRAPHICS, 0 },
    
    // Sound program ROM (Z80)
    { "wl_09.12b",     0x010000, 0xf6b3d060, ROMType::SOUND_PROGRAM, 0 },
    
    // Sound samples (OKI6295)
    { "wl_18.11c",     0x020000, 0xbde23d4d, ROMType::SOUND_SAMPLE, 0 },
    { "wl_19.12c",     0x020000, 0x683898f5, ROMType::SOUND_SAMPLE, 0 },
    
    // A-board PLDs (optional)
    { "buf1",          0x000117, 0xeb122de7, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "ioa1",          0x000117, 0x59c7ee3b, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "prg1",          0x000117, 0xf1129744, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "rom1",          0x000117, 0x41dc73b9, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "sou1",          0x000117, 0x84f4b2fe, ROMType::PLD, ROM_FLAG_OPTIONAL },
    
    // B-board PLDs (optional)
    { "wl24b.1a",      0x000117, 0x7101cdf1, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "lwio.11e",      0x000117, 0xad52b90c, ROMType::PLD, ROM_FLAG_OPTIONAL },
};

// Mercs (mercs)
static const ROMEntry mercs_roms[] = {
    // Program ROMs (68000)
    { "so2_30e.11f",   0x020000, 0xe17f9bf7, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "so2_35e.11h",   0x020000, 0x78e63575, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "so2_31e.12f",   0x020000, 0x51204d36, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "so2_36e.12h",   0x020000, 0x9cfba8b4, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "so2-32m.8h",    0x080000, 0x2eb5cf0c, ROMType::PROGRAM, 0 },
    
    // Graphics ROMs
    { "so2-6m.8a",     0x080000, 0xaa6102af, ROMType::GRAPHICS, 0 },
    { "so2-8m.10a",    0x080000, 0x839e6869, ROMType::GRAPHICS, 0 },
    { "so2-2m.4a",     0x080000, 0x597c2875, ROMType::GRAPHICS, 0 },
    { "so2-4m.6a",     0x080000, 0x912a9ca0, ROMType::GRAPHICS, 0 },
    { "so2_24.7d",     0x020000, 0x3f254efe, ROMType::GRAPHICS, 0 },
    { "so2_14.7c",     0x020000, 0xf5a8905e, ROMType::GRAPHICS, 0 },
    { "so2_26.9d",     0x020000, 0xf3aa5a4a, ROMType::GRAPHICS, 0 },
    { "so2_16.9c",     0x020000, 0xb43cd1a8, ROMType::GRAPHICS, 0 },
    { "so2_20.3d",     0x020000, 0x8ca751a3, ROMType::GRAPHICS, 0 },
    { "so2_10.3c",     0x020000, 0xe9f569fd, ROMType::GRAPHICS, 0 },
    { "so2_22.5d",     0x020000, 0xfce9a377, ROMType::GRAPHICS, 0 },
    { "so2_12.5c",     0x020000, 0xb7df8a06, ROMType::GRAPHICS, 0 },
    
    // Sound program ROM (Z80)
    { "so2_09.12b",    0x010000, 0xd09d7c7a, ROMType::SOUND_PROGRAM, 0 },
    
    // Sound samples (OKI6295)
    { "so2_18.11c",    0x020000, 0xbbea1643, ROMType::SOUND_SAMPLE, 0 },
    { "so2_19.12c",    0x020000, 0xac58aa71, ROMType::SOUND_SAMPLE, 0 },
    
    // A-board PLDs (optional)
    { "buf1",          0x000117, 0xeb122de7, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "ioa1",          0x000117, 0x59c7ee3b, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "prg1",          0x000117, 0xf1129744, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "rom1",          0x000117, 0x41dc73b9, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "sou1",          0x000117, 0x84f4b2fe, ROMType::PLD, ROM_FLAG_OPTIONAL },
    
    // B-board PLDs (optional)
    { "o224b.1a",      0x000117, 0xc211c8cd, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "iob1.11e",      0x000117, 0x3abc0700, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "c628",          0x000117, 0x662e090f, ROMType::PLD, ROM_FLAG_OPTIONAL },
};

// Varth: Operation Thunderstorm (varth)
static const ROMEntry varth_roms[] = {
    // Program ROMs (68000)
    { "vae_30b.11f",   0x020000, 0xadb8d391, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "vae_35b.11h",   0x020000, 0x44e5548f, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "vae_31b.12f",   0x020000, 0x1749a71c, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "vae_36b.12h",   0x020000, 0x5f2e2450, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "vae_28b.9f",    0x020000, 0xe524ca50, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "vae_33b.9h",    0x020000, 0xc0bbf8c9, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "vae_29b.10f",   0x020000, 0x6640996a, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "vae_34b.10h",   0x020000, 0xfa59be8a, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    
    // Graphics ROMs
    { "va-5m.7a",      0x080000, 0xb1fb726e, ROMType::GRAPHICS, 0 },
    { "va-7m.9a",      0x080000, 0x4c6588cd, ROMType::GRAPHICS, 0 },
    { "va-1m.3a",      0x080000, 0x0b1ace37, ROMType::GRAPHICS, 0 },
    { "va-3m.5a",      0x080000, 0x44dfe706, ROMType::GRAPHICS, 0 },
    
    // Sound program ROM (Z80)
    { "va_09.12b",     0x010000, 0x7a99446e, ROMType::SOUND_PROGRAM, 0 },
    
    // Sound samples (OKI6295)
    { "va_18.11c",     0x020000, 0xde30510e, ROMType::SOUND_SAMPLE, 0 },
    { "va_19.12c",     0x020000, 0x0610a4ac, ROMType::SOUND_SAMPLE, 0 },
    
    // A-board PLDs (optional)
    { "buf1",          0x000117, 0xeb122de7, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "ioa1",          0x000117, 0x59c7ee3b, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "prg1",          0x000117, 0xf1129744, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "rom1",          0x000117, 0x41dc73b9, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "sou1",          0x000117, 0x84f4b2fe, ROMType::PLD, ROM_FLAG_OPTIONAL },
    
    // B-board PLDs (optional)
    { "va24b.1a",      0x000117, 0xcc476650, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "iob1.11e",      0x000117, 0x3abc0700, ROMType::PLD, ROM_FLAG_OPTIONAL },
};

// Carrier Air Wing (cawing)
static const ROMEntry cawing_roms[] = {
    // Program ROMs (68000)
    { "cae_30a.11f",   0x020000, 0x91fceacd, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "cae_35a.11h",   0x020000, 0x3ef03083, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "cae_31a.12f",   0x020000, 0xe5b75caf, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "cae_36a.12h",   0x020000, 0xc73fd713, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "ca-32m.8h",     0x080000, 0x0c4837d4, ROMType::PROGRAM, 0 },
    
    // Graphics ROMs
    { "ca-5m.7a",      0x080000, 0x66d4cc37, ROMType::GRAPHICS, 0 },
    { "ca-7m.9a",      0x080000, 0xb6f896f2, ROMType::GRAPHICS, 0 },
    { "ca-1m.3a",      0x080000, 0x4d0620fd, ROMType::GRAPHICS, 0 },
    { "ca-3m.5a",      0x080000, 0x0b0341c3, ROMType::GRAPHICS, 0 },
    
    // Sound program ROM (Z80)
    { "ca_9.12b",      0x010000, 0x96fe7485, ROMType::SOUND_PROGRAM, 0 },
    
    // Sound samples (OKI6295)
    { "ca_18.11c",     0x020000, 0x4a613a2c, ROMType::SOUND_SAMPLE, 0 },
    { "ca_19.12c",     0x020000, 0x74584493, ROMType::SOUND_SAMPLE, 0 },
    
    // A-board PLDs (optional)
    { "buf1",          0x000117, 0xeb122de7, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "ioa1",          0x000117, 0x59c7ee3b, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "prg1",          0x000117, 0xf1129744, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "rom1",          0x000117, 0x41dc73b9, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "sou1",          0x000117, 0x84f4b2fe, ROMType::PLD, ROM_FLAG_OPTIONAL },
    
    // B-board PLDs (optional)
    { "ca24b.1a",      0x000117, 0x76ec0b1c, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "iob1.11e",      0x000117, 0x3abc0700, ROMType::PLD, ROM_FLAG_OPTIONAL },
};

// 1941: Counter Attack (1941)
static const ROMEntry game1941_roms[] = {
    // Program ROMs (68000)
    { "41em_30.11f",   0x020000, 0x4249ec61, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "41em_35.11h",   0x020000, 0xddbee5eb, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "41em_31.12f",   0x020000, 0x584e88e5, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "41em_36.12h",   0x020000, 0x3cfc31d0, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "41-32m.8h",     0x080000, 0x4e9648ca, ROMType::PROGRAM, 0 },
    
    // Graphics ROMs
    { "41-5m.7a",      0x080000, 0x01d1cb11, ROMType::GRAPHICS, 0 },
    { "41-7m.9a",      0x080000, 0xaeaa3509, ROMType::GRAPHICS, 0 },
    { "41-1m.3a",      0x080000, 0xff77985a, ROMType::GRAPHICS, 0 },
    { "41-3m.5a",      0x080000, 0x983be58f, ROMType::GRAPHICS, 0 },
    
    // Sound program ROM (Z80)
    { "41_9.12b",      0x010000, 0x0f9d8527, ROMType::SOUND_PROGRAM, 0 },
    
    // Sound samples (OKI6295)
    { "41_18.11c",     0x020000, 0xd1f15aeb, ROMType::SOUND_SAMPLE, 0 },
    { "41_19.12c",     0x020000, 0x15aec3a6, ROMType::SOUND_SAMPLE, 0 },
    
    // A-board PLDs (optional)
    { "buf1",          0x000117, 0xeb122de7, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "ioa1",          0x000117, 0x59c7ee3b, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "prg1",          0x000117, 0xf1129744, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "rom1",          0x000117, 0x41dc73b9, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "sou1",          0x000117, 0x84f4b2fe, ROMType::PLD, ROM_FLAG_OPTIONAL },
    
    // B-board PLDs (optional)
    { "yi24b.1a",      0x000117, 0x3004dcdf, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "iob1.11e",      0x000117, 0x3abc0700, ROMType::PLD, ROM_FLAG_OPTIONAL },
};

// Magic Sword (msword)
static const ROMEntry msword_roms[] = {
    // Program ROMs (68000)
    { "mse_30.11f",    0x020000, 0x03fc8dbc, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "mse_35.11h",    0x020000, 0xd5bf66cd, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "mse_31.12f",    0x020000, 0x30332bcf, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "mse_36.12h",    0x020000, 0x8f7d6ce9, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "ms-32m.8h",     0x080000, 0x2475ddfc, ROMType::PROGRAM, 0 },
    
    // Graphics ROMs
    { "ms-5m.7a",      0x080000, 0xc00fe7e2, ROMType::GRAPHICS, 0 },
    { "ms-7m.9a",      0x080000, 0x4ccacac5, ROMType::GRAPHICS, 0 },
    { "ms-1m.3a",      0x080000, 0x0d2bbe00, ROMType::GRAPHICS, 0 },
    { "ms-3m.5a",      0x080000, 0x3a1a5bf4, ROMType::GRAPHICS, 0 },
    
    // Sound program ROM (Z80)
    { "ms_09.12b",     0x010000, 0x57b29519, ROMType::SOUND_PROGRAM, 0 },
    
    // Sound samples (OKI6295)
    { "ms_18.11c",     0x020000, 0xfb64e90d, ROMType::SOUND_SAMPLE, 0 },
    { "ms_19.12c",     0x020000, 0x74f892b9, ROMType::SOUND_SAMPLE, 0 },
    
    // A-board PLDs (optional)
    { "buf1",          0x000117, 0xeb122de7, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "ioa1",          0x000117, 0x59c7ee3b, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "prg1",          0x000117, 0xf1129744, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "rom1",          0x000117, 0x41dc73b9, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "sou1",          0x000117, 0x84f4b2fe, ROMType::PLD, ROM_FLAG_OPTIONAL },
    
    // B-board PLDs (optional)
    { "ms24b.1a",      0x000117, 0x636dbe6d, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "iob1.11e",      0x000117, 0x3abc0700, ROMType::PLD, ROM_FLAG_OPTIONAL },
};

// DIP switch arrays
static const DIPInfo common_dips[] = {
    {0x01A, 0x00},
    {0x01C, 0x03},
    {0x01E, 0x60},
};

// Game database
const GameInfo GameDatabase::s_games[] = {
    {
        "Street Fighter II: The World Warrior",
        "sf2",
        sf2_roms,
        static_cast<u32>(sizeof(sf2_roms) / sizeof(sf2_roms[0])),
        CPSBoard::CPS_B_11,
        CPSMapper::MAPPER_STF29,
        common_dips,
        static_cast<u32>(sizeof(common_dips) / sizeof(common_dips[0]))
    },
    {
        "Street Fighter II: Champion Edition",
        "sf2ce",
        sf2ce_roms,
        static_cast<u32>(sizeof(sf2ce_roms) / sizeof(sf2ce_roms[0])),
        CPSBoard::CPS_B_21_DEF,
        CPSMapper::MAPPER_S9263B,
        common_dips,
        static_cast<u32>(sizeof(common_dips) / sizeof(common_dips[0]))
    },
    {
        "Street Fighter II: Hyper Fighting",
        "sf2hf",
        sf2hf_roms,
        static_cast<u32>(sizeof(sf2hf_roms) / sizeof(sf2hf_roms[0])),
        CPSBoard::CPS_B_21_DEF,
        CPSMapper::MAPPER_S9263B,
        common_dips,
        static_cast<u32>(sizeof(common_dips) / sizeof(common_dips[0])),
    },
    {
        "Three Wonders",
        "3wonders",
        threewonders_roms,
        static_cast<u32>(sizeof(threewonders_roms) / sizeof(threewonders_roms[0])),
        CPSBoard::CPS_B_21_BT1,
        CPSMapper::MAPPER_RT24B,
        common_dips,
        static_cast<u32>(sizeof(common_dips) / sizeof(common_dips[0]))
    },
    {
        "Captain Commando",
        "captcomm",
        captcomm_roms,
        static_cast<u32>(sizeof(captcomm_roms) / sizeof(captcomm_roms[0])),
        CPSBoard::CPS_B_21_BT3,
        CPSMapper::MAPPER_CC63B,
        common_dips,
        static_cast<u32>(sizeof(common_dips) / sizeof(common_dips[0]))
    },
    {
        "Knights of the Round",
        "knights",
        knights_roms,
        static_cast<u32>(sizeof(knights_roms) / sizeof(knights_roms[0])),
        CPSBoard::CPS_B_21_BT4,
        CPSMapper::MAPPER_KR63B,
        common_dips,
        static_cast<u32>(sizeof(common_dips) / sizeof(common_dips[0]))
    },
    {
        "The King of Dragons",
        "kod",
        kod_roms,
        static_cast<u32>(sizeof(kod_roms) / sizeof(kod_roms[0])),
        CPSBoard::CPS_B_21_BT2,
        CPSMapper::MAPPER_KD29B,
        common_dips,
        static_cast<u32>(sizeof(common_dips) / sizeof(common_dips[0]))
    },
    {
        "Nemo",
        "nemo",
        nemo_roms,
        static_cast<u32>(sizeof(nemo_roms) / sizeof(nemo_roms[0])),
        CPSBoard::CPS_B_15,
        CPSMapper::MAPPER_NM24B,
        common_dips,
        static_cast<u32>(sizeof(common_dips) / sizeof(common_dips[0]))
    },
    {
        "Final Fight",
        "ffight",
        ffight_roms,
        static_cast<u32>(sizeof(ffight_roms) / sizeof(ffight_roms[0])),
        CPSBoard::CPS_B_04,
        CPSMapper::MAPPER_S224B,
        common_dips,
        static_cast<u32>(sizeof(common_dips) / sizeof(common_dips[0]))
    },
    {
        "Warriors of Fate",
        "wof",
        wof_roms,
        static_cast<u32>(sizeof(wof_roms) / sizeof(wof_roms[0])),
        CPSBoard::CPS_B_21_QS1,
        CPSMapper::MAPPER_TK263B,
        common_dips,
        static_cast<u32>(sizeof(common_dips) / sizeof(common_dips[0]))
    },
    {
        "The Punisher",
        "punisher",
        punisher_roms,
        static_cast<u32>(sizeof(punisher_roms) / sizeof(punisher_roms[0])),
        CPSBoard::CPS_B_21_QS3,
        CPSMapper::MAPPER_PS63B,
        common_dips,
        static_cast<u32>(sizeof(common_dips) / sizeof(common_dips[0]))
    },
    {
        "Mega Man: The Power Battle",
        "megaman",
        megaman_roms,
        static_cast<u32>(sizeof(megaman_roms) / sizeof(megaman_roms[0])),
        CPSBoard::CPS_B_21_DEF,
        CPSMapper::MAPPER_RCM63B,
        common_dips,
        static_cast<u32>(sizeof(common_dips) / sizeof(common_dips[0]))
    },
    {
        "Willow",
        "willow",
        willow_roms,
        static_cast<u32>(sizeof(willow_roms) / sizeof(willow_roms[0])),
        CPSBoard::CPS_B_03,
        CPSMapper::MAPPER_WL24B,
        common_dips,
        static_cast<u32>(sizeof(common_dips) / sizeof(common_dips[0]))
    },
    {
        "Mercs",
        "mercs",
        mercs_roms,
        static_cast<u32>(sizeof(mercs_roms) / sizeof(mercs_roms[0])),
        CPSBoard::CPS_B_12,
        CPSMapper::MAPPER_O224B,
        common_dips,
        static_cast<u32>(sizeof(common_dips) / sizeof(common_dips[0]))
    },
    {
        "Varth: Operation Thunderstorm",
        "varth",
        varth_roms,
        static_cast<u32>(sizeof(varth_roms) / sizeof(varth_roms[0])),
        CPSBoard::CPS_B_04,
        CPSMapper::MAPPER_VA63B,
        common_dips,
        static_cast<u32>(sizeof(common_dips) / sizeof(common_dips[0]))
    },
    {
        "Carrier Air Wing",
        "cawing",
        cawing_roms,
        static_cast<u32>(sizeof(cawing_roms) / sizeof(cawing_roms[0])),
        CPSBoard::CPS_B_16,
        CPSMapper::MAPPER_CA24B,
        common_dips,
        static_cast<u32>(sizeof(common_dips) / sizeof(common_dips[0]))
    },
    {
        "1941: Counter Attack",
        "1941",
        game1941_roms,
        static_cast<u32>(sizeof(game1941_roms) / sizeof(game1941_roms[0])),
        CPSBoard::CPS_B_05,
        CPSMapper::MAPPER_YI24B,
        common_dips,
        static_cast<u32>(sizeof(common_dips) / sizeof(common_dips[0]))
    },
    {
        "Magic Sword",
        "msword",
        msword_roms,
        static_cast<u32>(sizeof(msword_roms) / sizeof(msword_roms[0])),
        CPSBoard::CPS_B_13,
        CPSMapper::MAPPER_MS24B,
        common_dips,
        static_cast<u32>(sizeof(common_dips) / sizeof(common_dips[0]))
    },
};

const u32 GameDatabase::s_gameCount = static_cast<u32>(sizeof(s_games) / sizeof(s_games[0]));

BoardConfig GameDatabase::getBoardConfig(CPSBoard board) {
    BoardConfig config = {};
    
    switch (board) {
        case CPSBoard::CPS_B_11:
            config.boardIdOffset = 0x72;
            config.boardIdValue1 = 0x04;
            config.boardIdValue2 = 0x01;
            config.layerControlReg = 0x66;
            config.paletteControlReg = 0x70;
            config.maskAddr[0] = 0x68;
            config.maskAddr[1] = 0x6a;
            config.maskAddr[2] = 0x6c;
            config.maskAddr[3] = 0x6e;
            config.layerEnable[0] = 0x08;
            config.layerEnable[1] = 0x10;
            config.layerEnable[2] = 0x20;
            break;
            
        case CPSBoard::CPS_B_21_DEF:
            config.boardIdOffset = 0x32;
            config.boardIdValue1 = 0x00;
            config.boardIdValue2 = 0x00;
            config.layerControlReg = 0x66;
            config.paletteControlReg = 0x70;
            config.maskAddr[0] = 0x68;
            config.maskAddr[1] = 0x6a;
            config.maskAddr[2] = 0x6c;
            config.maskAddr[3] = 0x6e;
            config.layerEnable[0] = 0x02;
            config.layerEnable[1] = 0x04;
            config.layerEnable[2] = 0x08;
            break;
            
        case CPSBoard::CPS_B_21_BT1:
            config.boardIdOffset = 0x72;
            config.boardIdValue1 = 0x08;
            config.boardIdValue2 = 0x00;
            config.layerControlReg = 0x68;
            config.paletteControlReg = 0x70;
            config.maskAddr[0] = 0x66;
            config.maskAddr[1] = 0x64;
            config.maskAddr[2] = 0x62;
            config.maskAddr[3] = 0x60;
            config.layerEnable[0] = 0x20;
            config.layerEnable[1] = 0x04;
            config.layerEnable[2] = 0x08;
            break;
            
        case CPSBoard::CPS_B_21_BT2:
            config.boardIdOffset = 0x00;
            config.boardIdValue1 = 0x00;
            config.boardIdValue2 = 0x00;
            config.layerControlReg = 0x60;
            config.paletteControlReg = 0x70;
            config.maskAddr[0] = 0x6e;
            config.maskAddr[1] = 0x6c;
            config.maskAddr[2] = 0x6a;
            config.maskAddr[3] = 0x68;
            config.layerEnable[0] = 0x30;
            config.layerEnable[1] = 0x08;
            config.layerEnable[2] = 0x30;
            break;
            
        case CPSBoard::CPS_B_21_BT3:
            config.boardIdOffset = 0x00;
            config.boardIdValue1 = 0x00;
            config.boardIdValue2 = 0x00;
            config.layerControlReg = 0x60;
            config.paletteControlReg = 0x70;
            config.maskAddr[0] = 0x6e;
            config.maskAddr[1] = 0x6c;
            config.maskAddr[2] = 0x6a;
            config.maskAddr[3] = 0x68;
            config.layerEnable[0] = 0x20;
            config.layerEnable[1] = 0x12;
            config.layerEnable[2] = 0x12;
            break;
            
        case CPSBoard::CPS_B_21_BT4:
            config.boardIdOffset = 0x00;
            config.boardIdValue1 = 0x00;
            config.boardIdValue2 = 0x00;
            config.layerControlReg = 0x68;
            config.paletteControlReg = 0x70;
            config.maskAddr[0] = 0x66;
            config.maskAddr[1] = 0x64;
            config.maskAddr[2] = 0x62;
            config.maskAddr[3] = 0x60;
            config.layerEnable[0] = 0x20;
            config.layerEnable[1] = 0x10;
            config.layerEnable[2] = 0x02;
            break;
            
        case CPSBoard::CPS_B_15:
            config.boardIdOffset = 0x4e;
            config.boardIdValue1 = 0x04;
            config.boardIdValue2 = 0x05;
            config.layerControlReg = 0x42;
            config.paletteControlReg = 0x4c;
            config.maskAddr[0] = 0x44;
            config.maskAddr[1] = 0x46;
            config.maskAddr[2] = 0x48;
            config.maskAddr[3] = 0x4a;
            config.layerEnable[0] = 0x04;
            config.layerEnable[1] = 0x02;
            config.layerEnable[2] = 0x20;
            break;
            
        case CPSBoard::CPS_B_03:
            config.boardIdOffset = 0x00;
            config.boardIdValue1 = 0x00;
            config.boardIdValue2 = 0x00;
            config.layerControlReg = 0x70;
            config.paletteControlReg = 0x66;
            config.maskAddr[0] = 0x6e;
            config.maskAddr[1] = 0x6c;
            config.maskAddr[2] = 0x6a;
            config.maskAddr[3] = 0x68;
            config.layerEnable[0] = 0x20;
            config.layerEnable[1] = 0x10;
            config.layerEnable[2] = 0x08;
            break;
            
        case CPSBoard::CPS_B_04:
            config.boardIdOffset = 0x60;
            config.boardIdValue1 = 0x00;
            config.boardIdValue2 = 0x04;
            config.layerControlReg = 0x6e;
            config.paletteControlReg = 0x6a;
            config.maskAddr[0] = 0x66;
            config.maskAddr[1] = 0x70;
            config.maskAddr[2] = 0x68;
            config.maskAddr[3] = 0x72;
            config.layerEnable[0] = 0x02;
            config.layerEnable[1] = 0x04;
            config.layerEnable[2] = 0x08;
            break;
            
        case CPSBoard::CPS_B_05:
            config.boardIdOffset = 0x60;
            config.boardIdValue1 = 0x00;
            config.boardIdValue2 = 0x05;
            config.layerControlReg = 0x68;
            config.paletteControlReg = 0x72;
            config.maskAddr[0] = 0x6a;
            config.maskAddr[1] = 0x6c;
            config.maskAddr[2] = 0x6e;
            config.maskAddr[3] = 0x70;
            config.layerEnable[0] = 0x02;
            config.layerEnable[1] = 0x08;
            config.layerEnable[2] = 0x20;
            break;
            
        case CPSBoard::CPS_B_12:
            config.boardIdOffset = 0x60;
            config.boardIdValue1 = 0x04;
            config.boardIdValue2 = 0x02;
            config.layerControlReg = 0x6c;
            config.paletteControlReg = 0x62;
            config.maskAddr[0] = 0x6a;
            config.maskAddr[1] = 0x68;
            config.maskAddr[2] = 0x66;
            config.maskAddr[3] = 0x64;
            config.layerEnable[0] = 0x02;
            config.layerEnable[1] = 0x04;
            config.layerEnable[2] = 0x08;
            break;
            
        case CPSBoard::CPS_B_13:
            config.boardIdOffset = 0x6e;
            config.boardIdValue1 = 0x04;
            config.boardIdValue2 = 0x03;
            config.layerControlReg = 0x62;
            config.paletteControlReg = 0x6c;
            config.maskAddr[0] = 0x64;
            config.maskAddr[1] = 0x66;
            config.maskAddr[2] = 0x68;
            config.maskAddr[3] = 0x6a;
            config.layerEnable[0] = 0x20;
            config.layerEnable[1] = 0x02;
            config.layerEnable[2] = 0x04;
            break;
            
        case CPSBoard::CPS_B_16:
            config.boardIdOffset = 0x40;
            config.boardIdValue1 = 0x04;
            config.boardIdValue2 = 0x06;
            config.layerControlReg = 0x4c;
            config.paletteControlReg = 0x42;
            config.maskAddr[0] = 0x4a;
            config.maskAddr[1] = 0x48;
            config.maskAddr[2] = 0x46;
            config.maskAddr[3] = 0x44;
            config.layerEnable[0] = 0x10;
            config.layerEnable[1] = 0x0a;
            config.layerEnable[2] = 0x0a;
            break;
            
        case CPSBoard::CPS_B_21_QS1:
            config.boardIdOffset = 0x00;
            config.boardIdValue1 = 0x00;
            config.boardIdValue2 = 0x00;
            config.layerControlReg = 0x62;
            config.paletteControlReg = 0x6c;
            config.maskAddr[0] = 0x64;
            config.maskAddr[1] = 0x66;
            config.maskAddr[2] = 0x68;
            config.maskAddr[3] = 0x6a;
            config.layerEnable[0] = 0x10;
            config.layerEnable[1] = 0x08;
            config.layerEnable[2] = 0x04;
            break;
            
        case CPSBoard::CPS_B_21_QS3:
            config.boardIdOffset = 0x4e;
            config.boardIdValue1 = 0x0c;
            config.boardIdValue2 = 0x00;
            config.layerControlReg = 0x52;
            config.paletteControlReg = 0x4c;
            config.maskAddr[0] = 0x54;
            config.maskAddr[1] = 0x56;
            config.maskAddr[2] = 0x48;
            config.maskAddr[3] = 0x4a;
            config.layerEnable[0] = 0x04;
            config.layerEnable[1] = 0x02;
            config.layerEnable[2] = 0x20;
            break;
            
        default:
            throw std::runtime_error("Unsupported board type");
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

const GfxRange* GameDatabase::getGfxMapperTable(CPSMapper mapper) {
    switch (mapper) {
        case CPSMapper::MAPPER_STF29:
            return mapper_STF29_table;
        case CPSMapper::MAPPER_S9263B:
            return mapper_S9263B_table;
        case CPSMapper::MAPPER_NM24B:
            return mapper_NM24B_table;
        case CPSMapper::MAPPER_RT24B:
            return mapper_RT24B_table;
        case CPSMapper::MAPPER_KD29B:
            return mapper_KD29B_table;
        case CPSMapper::MAPPER_CC63B:
            return mapper_CC63B_table;
        case CPSMapper::MAPPER_KR63B:
            return mapper_KR63B_table;
        case CPSMapper::MAPPER_CP1B1F:
            return mapper_CP1B1F_table;
        case CPSMapper::MAPPER_S224B:
            return mapper_S224B_table;
        case CPSMapper::MAPPER_TK263B:
            return mapper_TK263B_table;
        case CPSMapper::MAPPER_PS63B:
            return mapper_PS63B_table;
        case CPSMapper::MAPPER_RCM63B:
            return mapper_RCM63B_table;
        case CPSMapper::MAPPER_WL24B:
            return mapper_WL24B_table;
        case CPSMapper::MAPPER_O224B:
            return mapper_O224B_table;
        case CPSMapper::MAPPER_VA63B:
            return mapper_VA63B_table;
        case CPSMapper::MAPPER_CA24B:
            return mapper_CA24B_table;
        case CPSMapper::MAPPER_YI24B:
            return mapper_YI24B_table;
        case CPSMapper::MAPPER_MS24B:
            return mapper_MS24B_table;
        default:
            return nullptr;
    }
}

void GameDatabase::getGfxBankSizes(CPSMapper mapper, u32 sizes[4]) {
    switch (mapper) {
        case CPSMapper::MAPPER_STF29:
            sizes[0] = 0x08000;
            sizes[1] = 0x08000;
            sizes[2] = 0x08000;
            sizes[3] = 0x00000;
            break;
        case CPSMapper::MAPPER_S9263B:
            sizes[0] = 0x08000;
            sizes[1] = 0x08000;
            sizes[2] = 0x08000;
            sizes[3] = 0x00000;
            break;
        case CPSMapper::MAPPER_NM24B:
            sizes[0] = 0x8000;
            sizes[1] = 0x0000;
            sizes[2] = 0x0000;
            sizes[3] = 0x0000;
            break;
        case CPSMapper::MAPPER_RT24B:
            sizes[0] = 0x8000;
            sizes[1] = 0x8000;
            sizes[2] = 0x0000;
            sizes[3] = 0x0000;
            break;
        case CPSMapper::MAPPER_KD29B:
            sizes[0] = 0x8000;
            sizes[1] = 0x8000;
            sizes[2] = 0x0000;
            sizes[3] = 0x0000;
            break;
        case CPSMapper::MAPPER_CC63B:
            sizes[0] = 0x8000;
            sizes[1] = 0x8000;
            sizes[2] = 0x0000;
            sizes[3] = 0x0000;
            break;
        case CPSMapper::MAPPER_KR63B:
            sizes[0] = 0x8000;
            sizes[1] = 0x8000;
            sizes[2] = 0x0000;
            sizes[3] = 0x0000;
            break;
        case CPSMapper::MAPPER_CP1B1F:
            sizes[0] = 0x10000;
            sizes[1] = 0x00000;
            sizes[2] = 0x00000;
            sizes[3] = 0x00000;
            break;
        case CPSMapper::MAPPER_S224B:
            sizes[0] = 0x8000;
            sizes[1] = 0x0000;
            sizes[2] = 0x0000;
            sizes[3] = 0x0000;
            break;
        case CPSMapper::MAPPER_TK263B:
            sizes[0] = 0x8000;
            sizes[1] = 0x8000;
            sizes[2] = 0x0000;
            sizes[3] = 0x0000;
            break;
        case CPSMapper::MAPPER_PS63B:
            sizes[0] = 0x8000;
            sizes[1] = 0x8000;
            sizes[2] = 0x0000;
            sizes[3] = 0x0000;
            break;
        case CPSMapper::MAPPER_RCM63B:
            sizes[0] = 0x8000;
            sizes[1] = 0x8000;
            sizes[2] = 0x8000;
            sizes[3] = 0x8000;
            break;
        case CPSMapper::MAPPER_WL24B:
            sizes[0] = 0x8000;
            sizes[1] = 0x8000;
            sizes[2] = 0x0000;
            sizes[3] = 0x0000;
            break;
        case CPSMapper::MAPPER_O224B:
            sizes[0] = 0x8000;
            sizes[1] = 0x4000;
            sizes[2] = 0x0000;
            sizes[3] = 0x0000;
            break;
        case CPSMapper::MAPPER_VA63B:
            sizes[0] = 0x8000;
            sizes[1] = 0x0000;
            sizes[2] = 0x0000;
            sizes[3] = 0x0000;
            break;
        case CPSMapper::MAPPER_CA24B:
            sizes[0] = 0x8000;
            sizes[1] = 0x0000;
            sizes[2] = 0x0000;
            sizes[3] = 0x0000;
            break;
        case CPSMapper::MAPPER_YI24B:
            sizes[0] = 0x8000;
            sizes[1] = 0x0000;
            sizes[2] = 0x0000;
            sizes[3] = 0x0000;
            break;
        case CPSMapper::MAPPER_MS24B:
            sizes[0] = 0x8000;
            sizes[1] = 0x0000;
            sizes[2] = 0x0000;
            sizes[3] = 0x0000;
            break;
        default:
            sizes[0] = 0;
            sizes[1] = 0;
            sizes[2] = 0;
            sizes[3] = 0;
            break;
    }
}

} // namespace cps1
