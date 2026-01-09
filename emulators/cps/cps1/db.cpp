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

// Three Wonders (3wonders)
static const ROMEntry threewonders_roms[] = {
    // Program ROMs (68000, byteswapped)
    { "rte_30a.11f",   0x020000, 0xef5b8b33, ROMType::PROGRAM, 0 },
    { "rte_35a.11h",   0x020000, 0x7d705529, ROMType::PROGRAM, 0 },
    { "rte_31a.12f",   0x020000, 0x32835e5e, ROMType::PROGRAM, 0 },
    { "rte_36a.12h",   0x020000, 0x7637975f, ROMType::PROGRAM, 0 },
    { "rt_28a.9f",     0x020000, 0x054137c8, ROMType::PROGRAM, 0 },
    { "rt_33a.9h",     0x020000, 0x7264cb1b, ROMType::PROGRAM, 0 },
    { "rte_29a.10f",   0x020000, 0xcddaa919, ROMType::PROGRAM, 0 },
    { "rte_34a.10h",   0x020000, 0xed52e7e5, ROMType::PROGRAM, 0 },
    
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
    // Program ROMs (68000, no byteswap for main ROMs, byteswap for last 2)
    { "cce_23f.8f",    0x080000, 0x42c814c5, ROMType::PROGRAM, 0 },
    { "cc_22f.7f",     0x080000, 0x0fd34195, ROMType::PROGRAM, 0 },
    { "cc_24f.9e",     0x020000, 0x3a794f25, ROMType::PROGRAM, 0 },
    { "cc_28f.9f",     0x020000, 0xfc3c2906, ROMType::PROGRAM, 0 },
    
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
    // Program ROMs (68000, no byteswap)
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
    // Program ROMs (68000, byteswapped)
    { "kde_30a.11e",   0x020000, 0xfcb5efe2, ROMType::PROGRAM, 0 },
    { "kde_37a.11f",   0x020000, 0xf22e5266, ROMType::PROGRAM, 0 },
    { "kde_31a.12e",   0x020000, 0xc710d722, ROMType::PROGRAM, 0 },
    { "kde_38a.12f",   0x020000, 0x57d6ed3a, ROMType::PROGRAM, 0 },
    { "kd_28.9e",      0x020000, 0x9367bcd9, ROMType::PROGRAM, 0 },
    { "kd_35.9f",      0x020000, 0x4ca6a48a, ROMType::PROGRAM, 0 },
    { "kd_29.10e",     0x020000, 0x0360fa72, ROMType::PROGRAM, 0 },
    { "kd_36a.10f",    0x020000, 0x95a3cef8, ROMType::PROGRAM, 0 },
    
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
    // Program ROMs (68000, byteswapped for first 4, no byteswap for last)
    { "nme_30a.11f",   0x020000, 0xd2c03e56, ROMType::PROGRAM, 0 },
    { "nme_35a.11h",   0x020000, 0x5fd31661, ROMType::PROGRAM, 0 },
    { "nme_31a.12f",   0x020000, 0xb2bd4f6f, ROMType::PROGRAM, 0 },
    { "nme_36a.12h",   0x020000, 0xee9450e3, ROMType::PROGRAM, 0 },
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

// Pang! 3 (pang3)
static const ROMEntry pang3_roms[] = {
    // Program ROMs (68000, no byteswap)
    { "pa3e_17a.11l",  0x080000, 0xa213fa80, ROMType::PROGRAM, 0 },
    { "pa3e_16a.10l",  0x080000, 0x7169ea67, ROMType::PROGRAM, 0 },
    
    // Graphics ROMs
    { "pa3-01m.2c",    0x200000, 0x068a152c, ROMType::GRAPHICS, 0 },
    { "pa3-07m.2f",    0x200000, 0x3a4a619d, ROMType::GRAPHICS, 0 },
    
    // Sound program ROM (Z80)
    { "pa3_11.11f",    0x020000, 0xcb1423a2, ROMType::SOUND_PROGRAM, 0 },
    
    // Sound samples (OKI6295)
    { "pa3_05.10d",    0x020000, 0x73a10d5d, ROMType::SOUND_SAMPLE, 0 },
    { "pa3_06.11d",    0x020000, 0xaffa4f82, ROMType::SOUND_SAMPLE, 0 },
    
    // A-board PLDs (optional)
    { "buf1",          0x000117, 0xeb122de7, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "ioa1",          0x000117, 0x59c7ee3b, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "prg1",          0x000117, 0xf1129744, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "rom1",          0x000117, 0x41dc73b9, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "sou1",          0x000117, 0x84f4b2fe, ROMType::PLD, ROM_FLAG_OPTIONAL },
    
    // B-board PLDs (optional)
    { "cp1b1f.1f",     0x000117, 0x3979b8e3, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "cp1b8k.8k",     0x000117, 0x8a52ea7a, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "cp1b9ka.9k",    0x000117, 0x238d3ff4, ROMType::PLD, ROM_FLAG_OPTIONAL },
    
    // C-board PLDs (optional)
    { "ioc1.ic7",      0x000104, 0xa399772d, ROMType::PLD, ROM_FLAG_OPTIONAL },
    { "c632.ic1",      0x000117, 0x0fbd9270, ROMType::PLD, ROM_FLAG_OPTIONAL },
};

// Game database
const GameInfo GameDatabase::s_games[] = {
    {
        "Street Fighter II: The World Warrior",
        "sf2",
        sf2_roms,
        static_cast<u32>(sizeof(sf2_roms) / sizeof(sf2_roms[0])),
        true,
        CPSBoard::CPS_B_11,
        CPSMapper::MAPPER_STF29,
        0x00,
        0x03,
        0x60,
        0xFF
    },
    {
        "Street Fighter II: Champion Edition",
        "sf2ce",
        sf2ce_roms,
        static_cast<u32>(sizeof(sf2ce_roms) / sizeof(sf2ce_roms[0])),
        false,
        CPSBoard::CPS_B_21_DEF,
        CPSMapper::MAPPER_S9263B,
        0x00,
        0x03,
        0x60,
        0xFF
    },
    {
        "Three Wonders",
        "3wonders",
        threewonders_roms,
        static_cast<u32>(sizeof(threewonders_roms) / sizeof(threewonders_roms[0])),
        true,
        CPSBoard::CPS_B_21_BT1,
        CPSMapper::MAPPER_RT24B,
        0x00,
        0x03,
        0x60,
        0xFF
    },
    {
        "Captain Commando",
        "captcomm",
        captcomm_roms,
        static_cast<u32>(sizeof(captcomm_roms) / sizeof(captcomm_roms[0])),
        false,
        CPSBoard::CPS_B_21_BT3,
        CPSMapper::MAPPER_CC63B,
        0x00,
        0x03,
        0x60,
        0xFF
    },
    {
        "Knights of the Round",
        "knights",
        knights_roms,
        static_cast<u32>(sizeof(knights_roms) / sizeof(knights_roms[0])),
        false,
        CPSBoard::CPS_B_21_BT4,
        CPSMapper::MAPPER_KR63B,
        0x00,
        0x03,
        0x60,
        0xFF
    },
    {
        "The King of Dragons",
        "kod",
        kod_roms,
        static_cast<u32>(sizeof(kod_roms) / sizeof(kod_roms[0])),
        true,
        CPSBoard::CPS_B_21_BT2,
        CPSMapper::MAPPER_KD29B,
        0x00,
        0x03,
        0x60,
        0xFF
    },
    {
        "Nemo",
        "nemo",
        nemo_roms,
        static_cast<u32>(sizeof(nemo_roms) / sizeof(nemo_roms[0])),
        true,
        CPSBoard::CPS_B_15,
        CPSMapper::MAPPER_NM24B,
        0x00,
        0x03,
        0x60,
        0xFF
    },
    {
        "Pang! 3",
        "pang3",
        pang3_roms,
        static_cast<u32>(sizeof(pang3_roms) / sizeof(pang3_roms[0])),
        false,
        CPSBoard::CPS_B_21_DEF,
        CPSMapper::MAPPER_CP1B1F,
        0x00,
        0x03,
        0x60,
        0xFF
    },
};

const u32 GameDatabase::s_gameCount = static_cast<u32>(sizeof(s_games) / sizeof(s_games[0]));

BoardConfig GameDatabase::getBoardConfig(CPSBoard board) {
    BoardConfig config = {};
    
    switch (board) {
        case CPSBoard::CPS_B_11:
            config.boardIdOffset = 0x82;
            config.boardIdValue1 = 0x40;
            config.boardIdValue2 = 0x43;
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

} // namespace cps1
