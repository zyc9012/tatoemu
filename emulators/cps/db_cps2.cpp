#include "db.h"
#include "../../../utilities/miniz/miniz.h"
#include <algorithm>
#include <cstring>

namespace cps {

// ============================================================================
// ROM Definitions
// ============================================================================

// 19XX: The War Against Destiny
static const ROMEntry game_19xx_roms[] = {
    { "19xe.03b",      0x080000, 0x9bf9d9b1, ROMType::PROGRAM, 0 },
    { "19xe.04b",      0x080000, 0x9aa46476, ROMType::PROGRAM, 0 },
    { "19xe.05b",      0x080000, 0xa04a2c5e, ROMType::PROGRAM, 0 },
    { "19xe.06b",      0x080000, 0xf0a81c33, ROMType::PROGRAM, 0 },
    { "19x.07",        0x080000, 0x61c0296c, ROMType::PROGRAM, 0 },
    { "19x.13m",       0x080000, 0x427aeb18, ROMType::GRAPHICS, 0 },
    { "19x.15m",       0x080000, 0x63bdbf54, ROMType::GRAPHICS, 0 },
    { "19x.17m",       0x080000, 0x2dfe18b5, ROMType::GRAPHICS, 0 },
    { "19x.19m",       0x080000, 0xcbef9579, ROMType::GRAPHICS, 0 },
    { "19x.14m",       0x200000, 0xe916967c, ROMType::GRAPHICS, 0 },
    { "19x.16m",       0x200000, 0x6e75f3db, ROMType::GRAPHICS, 0 },
    { "19x.18m",       0x200000, 0x2213e798, ROMType::GRAPHICS, 0 },
    { "19x.20m",       0x200000, 0xab9d5b96, ROMType::GRAPHICS, 0 },
    { "19x.01",        0x020000, 0xef55195e, ROMType::SOUND_PROGRAM, 0 },
    { "19x.11m",       0x200000, 0xd38beef3, ROMType::SOUND_SAMPLE, 0 },
    { "19x.12m",       0x200000, 0xd47c96e2, ROMType::SOUND_SAMPLE, 0 },
    { "19xx.key",      0x000014, 0x6f5d6406, ROMType::ENCRYPTION_KEY, 0 },
};

// 1944: The Loop Master
static const ROMEntry game_1944_roms[] = {
    { "nffe.03",       0x080000, 0x7544b926, ROMType::PROGRAM, 0 },
    { "nffe.04",       0x080000, 0xdba1c66e, ROMType::PROGRAM, 0 },
    { "nffe.05",       0x080000, 0xd78d31d3, ROMType::PROGRAM, 0 },
    { "nff.13m",       0x400000, 0xc9fca741, ROMType::GRAPHICS, 0 },
    { "nff.15m",       0x400000, 0xf809d898, ROMType::GRAPHICS, 0 },
    { "nff.17m",       0x400000, 0x15ba4507, ROMType::GRAPHICS, 0 },
    { "nff.19m",       0x400000, 0x3dd41b8c, ROMType::GRAPHICS, 0 },
    { "nff.14m",       0x100000, 0x3fe3a54b, ROMType::GRAPHICS, 0 },
    { "nff.16m",       0x100000, 0x565cd231, ROMType::GRAPHICS, 0 },
    { "nff.18m",       0x100000, 0x63ca5988, ROMType::GRAPHICS, 0 },
    { "nff.20m",       0x100000, 0x21eb8f3b, ROMType::GRAPHICS, 0 },
    { "nff.01",        0x020000, 0xd2e44318, ROMType::SOUND_PROGRAM, 0 },
    { "nff.11m",       0x400000, 0x243e4e05, ROMType::SOUND_SAMPLE, 0 },
    { "nff.12m",       0x400000, 0x4fcf1600, ROMType::SOUND_SAMPLE, 0 },
    { "1944.key",      0x000014, 0x5f22140e, ROMType::ENCRYPTION_KEY, 0 },
};

// Alien vs. Predator
static const ROMEntry avsp_roms[] = {
    { "avpe.03d",      0x080000, 0x774334a9, ROMType::PROGRAM, 0 },
    { "avpe.04d",      0x080000, 0x7fa83769, ROMType::PROGRAM, 0 },
    { "avp.05d",       0x080000, 0xfbfb5d7a, ROMType::PROGRAM, 0 },
    { "avp.06",        0x080000, 0x190b817f, ROMType::PROGRAM, 0 },
    { "avp.13m",       0x200000, 0x8f8b5ae4, ROMType::GRAPHICS, 0 },
    { "avp.15m",       0x200000, 0xb00280df, ROMType::GRAPHICS, 0 },
    { "avp.17m",       0x200000, 0x94403195, ROMType::GRAPHICS, 0 },
    { "avp.19m",       0x200000, 0xe1981245, ROMType::GRAPHICS, 0 },
    { "avp.14m",       0x100000, 0x39933b1e, ROMType::GRAPHICS, 0 },
    { "avp.16m",       0x100000, 0x85412860, ROMType::GRAPHICS, 0 },
    { "avp.18m",       0x100000, 0x2e2beb06, ROMType::GRAPHICS, 0 },
    { "avp.20m",       0x100000, 0xe4798d1a, ROMType::GRAPHICS, 0 },
    { "avp.01",        0x020000, 0x2d3b4220, ROMType::SOUND_PROGRAM, 0 },
    { "avp.11m",       0x200000, 0x83499817, ROMType::SOUND_SAMPLE, 0 },
    { "avp.12m",       0x200000, 0xf4110d49, ROMType::SOUND_SAMPLE, 0 },
    { "avsp.key",      0x000014, 0xe69fa35b, ROMType::ENCRYPTION_KEY, 0 },
};

// Dungeons & Dragons: Shadow over Mystara
static const ROMEntry ddsom_roms[] = {
    { "dd2e.03e",      0x080000, 0x449361AF, ROMType::PROGRAM, 0 },
    { "dd2e.04e",      0x080000, 0x5B7052B6, ROMType::PROGRAM, 0 },
    { "dd2e.05e",      0x080000, 0x788D5F60, ROMType::PROGRAM, 0 },
    { "dd2e.06e",      0x080000, 0xE0807E1E, ROMType::PROGRAM, 0 },
    { "dd2e.07",       0x080000, 0xbb777a02, ROMType::PROGRAM, 0 },
    { "dd2e.08",       0x080000, 0x30970890, ROMType::PROGRAM, 0 },
    { "dd2e.09",       0x080000, 0x99e2194d, ROMType::PROGRAM, 0 },
    { "dd2e.10",       0x080000, 0xe198805e, ROMType::PROGRAM, 0 },
    { "dd2.13m",       0x400000, 0xa46b4e6e, ROMType::GRAPHICS, 0 },
    { "dd2.15m",       0x400000, 0xd5fc50fc, ROMType::GRAPHICS, 0 },
    { "dd2.17m",       0x400000, 0x837c0867, ROMType::GRAPHICS, 0 },
    { "dd2.19m",       0x400000, 0xbb0ec21c, ROMType::GRAPHICS, 0 },
    { "dd2.14m",       0x200000, 0x6d824ce2, ROMType::GRAPHICS, 0 },
    { "dd2.16m",       0x200000, 0x79682ae5, ROMType::GRAPHICS, 0 },
    { "dd2.18m",       0x200000, 0xacddd149, ROMType::GRAPHICS, 0 },
    { "dd2.20m",       0x200000, 0x117fb0c0, ROMType::GRAPHICS, 0 },
    { "dd2.01",        0x020000, 0x99d657e5, ROMType::SOUND_PROGRAM, 0 },
    { "dd2.02",        0x020000, 0x117a3824, ROMType::SOUND_PROGRAM, 0 },
    { "dd2.11m",       0x200000, 0x98d0c325, ROMType::SOUND_SAMPLE, 0 },
    { "dd2.12m",       0x200000, 0x5ea2e7fa, ROMType::SOUND_SAMPLE, 0 },
    { "ddsom.key",     0x000014, 0x541e425d, ROMType::ENCRYPTION_KEY, 0 },
};

// Dungeons & Dragons: Tower of Doom
static const ROMEntry ddtod_roms[] = {
    { "dade.03c",      0x080000, 0x8e73533d, ROMType::PROGRAM, 0 },
    { "dade.04c",      0x080000, 0x00c2e82e, ROMType::PROGRAM, 0 },
    { "dade.05c",      0x080000, 0xea996008, ROMType::PROGRAM, 0 },
    { "dad.06a",       0x080000, 0x6225495a, ROMType::PROGRAM, 0 },
    { "dad.07a",       0x080000, 0xb3480ec3, ROMType::PROGRAM, 0 },
    { "dad.13m",       0x200000, 0xda3cb7d6, ROMType::GRAPHICS, 0 },
    { "dad.15m",       0x200000, 0x92b63172, ROMType::GRAPHICS, 0 },
    { "dad.17m",       0x200000, 0xb98757f5, ROMType::GRAPHICS, 0 },
    { "dad.19m",       0x200000, 0x8121ce46, ROMType::GRAPHICS, 0 },
    { "dad.14m",       0x100000, 0x837e6f3f, ROMType::GRAPHICS, 0 },
    { "dad.16m",       0x100000, 0xf0916bdb, ROMType::GRAPHICS, 0 },
    { "dad.18m",       0x100000, 0xcef393ef, ROMType::GRAPHICS, 0 },
    { "dad.20m",       0x100000, 0x8953fe9e, ROMType::GRAPHICS, 0 },
    { "dad.01",        0x020000, 0x3f5e2424, ROMType::SOUND_PROGRAM, 0 },
    { "dad.11m",       0x200000, 0x0c499b67, ROMType::SOUND_SAMPLE, 0 },
    { "dad.12m",       0x200000, 0x2f0b5a4e, ROMType::SOUND_SAMPLE, 0 },
    { "ddtod.key",     0x000014, 0x41dfca41, ROMType::ENCRYPTION_KEY, 0 },
};

// Hyper Street Fighter II: The Anniversary Edition
static const ROMEntry hsf2_roms[] = {
    { "hs2u.03",       0x080000, 0xb308151e, ROMType::PROGRAM, 0 },
    { "hs2u.04",       0x080000, 0x327aa49c, ROMType::PROGRAM, 0 },
    { "hs2.05",        0x080000, 0xdde34a35, ROMType::PROGRAM, 0 },
    { "hs2.06",        0x080000, 0xf4e56dda, ROMType::PROGRAM, 0 },
    { "hs2.07",        0x080000, 0xee4420fc, ROMType::PROGRAM, 0 },
    { "hs2.08",        0x080000, 0xc9441533, ROMType::PROGRAM, 0 },
    { "hs2.09",        0x080000, 0x3fc638a8, ROMType::PROGRAM, 0 },
    { "hs2.10",        0x080000, 0x20d0f9e4, ROMType::PROGRAM, 0 },
    { "hs2.13m",       0x800000, 0xa6ecab17, ROMType::GRAPHICS, 0 },
    { "hs2.15m",       0x800000, 0x10a0ae4d, ROMType::GRAPHICS, 0 },
    { "hs2.17m",       0x800000, 0xadfa7726, ROMType::GRAPHICS, 0 },
    { "hs2.19m",       0x800000, 0xbb3ae322, ROMType::GRAPHICS, 0 },
    { "hs2.01",        0x020000, 0xc1a13786, ROMType::SOUND_PROGRAM, 0 },
    { "hs2.02",        0x020000, 0x2d8794aa, ROMType::SOUND_PROGRAM, 0 },
    { "hs2.11m",       0x800000, 0x0e15c359, ROMType::SOUND_SAMPLE, 0 },
    { "hsf2.key",      0x000014, 0xfc9b18c9, ROMType::ENCRYPTION_KEY, 0 },
};

// Marvel Super Heroes
static const ROMEntry msh_roms[] = {
    { "mshe.03e",      0x080000, 0xbd951414, ROMType::PROGRAM, 0 },
    { "mshe.04e",      0x080000, 0x19dd42f2, ROMType::PROGRAM, 0 },
    { "msh.05",        0x080000, 0x6a091b9e, ROMType::PROGRAM, 0 },
    { "msh.06b",       0x080000, 0x803e3fa4, ROMType::PROGRAM, 0 },
    { "msh.07a",       0x080000, 0xc45f8e27, ROMType::PROGRAM, 0 },
    { "msh.08a",       0x080000, 0x9ca6f12c, ROMType::PROGRAM, 0 },
    { "msh.09a",       0x080000, 0x82ec27af, ROMType::PROGRAM, 0 },
    { "msh.10b",       0x080000, 0x8d931196, ROMType::PROGRAM, 0 },
    { "msh.13m",       0x400000, 0x09d14566, ROMType::GRAPHICS, 0 },
    { "msh.15m",       0x400000, 0xee962057, ROMType::GRAPHICS, 0 },
    { "msh.17m",       0x400000, 0x604ece14, ROMType::GRAPHICS, 0 },
    { "msh.19m",       0x400000, 0x94a731e8, ROMType::GRAPHICS, 0 },
    { "msh.14m",       0x400000, 0x4197973e, ROMType::GRAPHICS, 0 },
    { "msh.16m",       0x400000, 0x438da4a0, ROMType::GRAPHICS, 0 },
    { "msh.18m",       0x400000, 0x4db92d94, ROMType::GRAPHICS, 0 },
    { "msh.20m",       0x400000, 0xa2b0c6c0, ROMType::GRAPHICS, 0 },
    { "msh.01",        0x020000, 0xc976e6f9, ROMType::SOUND_PROGRAM, 0 },
    { "msh.02",        0x020000, 0xce67d0d9, ROMType::SOUND_PROGRAM, 0 },
    { "msh.11m",       0x200000, 0x37ac6d30, ROMType::SOUND_SAMPLE, 0 },
    { "msh.12m",       0x200000, 0xde092570, ROMType::SOUND_SAMPLE, 0 },
    { "msh.key",       0x000014, 0xb494368e, ROMType::ENCRYPTION_KEY, 0 },
};

// Marvel Super Heroes vs. Street Fighter
static const ROMEntry mshvsf_roms[] = {
    { "mvse.03f",      0x080000, 0xb72dc199, ROMType::PROGRAM, 0 },
    { "mvse.04f",      0x080000, 0x6ef799f9, ROMType::PROGRAM, 0 },
    { "mvs.05a",       0x080000, 0x1a5de0cb, ROMType::PROGRAM, 0 },
    { "mvs.06a",       0x080000, 0x959f3030, ROMType::PROGRAM, 0 },
    { "mvs.07b",       0x080000, 0x7f915bdb, ROMType::PROGRAM, 0 },
    { "mvs.08a",       0x080000, 0xc2813884, ROMType::PROGRAM, 0 },
    { "mvs.09b",       0x080000, 0x3ba08818, ROMType::PROGRAM, 0 },
    { "mvs.10b",       0x080000, 0xcf0dba98, ROMType::PROGRAM, 0 },
    { "mvs.13m",       0x400000, 0x29b05fd9, ROMType::GRAPHICS, 0 },
    { "mvs.15m",       0x400000, 0xfaddccf1, ROMType::GRAPHICS, 0 },
    { "mvs.17m",       0x400000, 0x97aaf4c7, ROMType::GRAPHICS, 0 },
    { "mvs.19m",       0x400000, 0xcb70e915, ROMType::GRAPHICS, 0 },
    { "mvs.14m",       0x400000, 0xb3b1972d, ROMType::GRAPHICS, 0 },
    { "mvs.16m",       0x400000, 0x08aadb5d, ROMType::GRAPHICS, 0 },
    { "mvs.18m",       0x400000, 0xc1228b35, ROMType::GRAPHICS, 0 },
    { "mvs.20m",       0x400000, 0x366cc6c2, ROMType::GRAPHICS, 0 },
    { "mvs.01",        0x020000, 0x68252324, ROMType::SOUND_PROGRAM, 0 },
    { "mvs.02",        0x020000, 0xb34e773d, ROMType::SOUND_PROGRAM, 0 },
    { "mvs.11m",       0x400000, 0x86219770, ROMType::SOUND_SAMPLE, 0 },
    { "mvs.12m",       0x400000, 0xf2fd7f68, ROMType::SOUND_SAMPLE, 0 },
    { "mshvsf.key",    0x000014, 0x64660867, ROMType::ENCRYPTION_KEY, 0 },
};

// Marvel vs. Capcom: Clash of Super Heroes
static const ROMEntry mvsc_roms[] = {
    { "mvce.03a",      0x080000, 0x824e4a90, ROMType::PROGRAM, 0 },
    { "mvce.04a",      0x080000, 0x436c5a4e, ROMType::PROGRAM, 0 },
    { "mvc.05a",       0x080000, 0x2d8c8e86, ROMType::PROGRAM, 0 },
    { "mvc.06a",       0x080000, 0x8528e1f5, ROMType::PROGRAM, 0 },
    { "mvc.07",        0x080000, 0xc3baa32b, ROMType::PROGRAM, 0 },
    { "mvc.08",        0x080000, 0xbc002fcd, ROMType::PROGRAM, 0 },
    { "mvc.09",        0x080000, 0xc67b26df, ROMType::PROGRAM, 0 },
    { "mvc.10",        0x080000, 0x0fdd1e26, ROMType::PROGRAM, 0 },
    { "mvc.13m",       0x400000, 0xfa5f74bc, ROMType::GRAPHICS, 0 },
    { "mvc.15m",       0x400000, 0x71938a8f, ROMType::GRAPHICS, 0 },
    { "mvc.17m",       0x400000, 0x92741d07, ROMType::GRAPHICS, 0 },
    { "mvc.19m",       0x400000, 0xbcb72fc6, ROMType::GRAPHICS, 0 },
    { "mvc.14m",       0x400000, 0x7f1df4e4, ROMType::GRAPHICS, 0 },
    { "mvc.16m",       0x400000, 0x90bd3203, ROMType::GRAPHICS, 0 },
    { "mvc.18m",       0x400000, 0x67aaf727, ROMType::GRAPHICS, 0 },
    { "mvc.20m",       0x400000, 0x8b0bade8, ROMType::GRAPHICS, 0 },
    { "mvc.01",        0x020000, 0x41629e95, ROMType::SOUND_PROGRAM, 0 },
    { "mvc.02",        0x020000, 0x963abf6b, ROMType::SOUND_PROGRAM, 0 },
    { "mvc.11m",       0x400000, 0x850fe663, ROMType::SOUND_SAMPLE, 0 },
    { "mvc.12m",       0x400000, 0x7ccb1896, ROMType::SOUND_SAMPLE, 0 },
    { "mvsc.key",      0x000014, 0x7e101e09, ROMType::ENCRYPTION_KEY, 0 },
};

// Progear
static const ROMEntry progear_roms[] = {
    { "pgau.03",       0x080000, 0x343a783e, ROMType::PROGRAM, 0 },
    { "pgau.04",       0x080000, 0x16208d79, ROMType::PROGRAM, 0 },
    { "pga-simm.01c",  0x200000, 0x452f98b0, ROMType::GRAPHICS_SIMM, 0 },
    { "pga-simm.01d",  0x200000, 0x9e672092, ROMType::GRAPHICS_SIMM, 0 },
    { "pga-simm.01a",  0x200000, 0xae9ddafe, ROMType::GRAPHICS_SIMM, 0 },
    { "pga-simm.01b",  0x200000, 0x94d72D94, ROMType::GRAPHICS_SIMM, 0 },
    { "pga-simm.03c",  0x200000, 0x48a1886d, ROMType::GRAPHICS_SIMM, 0 },
    { "pga-simm.03d",  0x200000, 0x172d7e37, ROMType::GRAPHICS_SIMM, 0 },
    { "pga-simm.03a",  0x200000, 0x9ee33d98, ROMType::GRAPHICS_SIMM, 0 },
    { "pga-simm.03b",  0x200000, 0x848dee32, ROMType::GRAPHICS_SIMM, 0 },
    { "pga.01",        0x020000, 0xbdbfa992, ROMType::SOUND_PROGRAM, 0 },
    { "pga-simm.05a",  0x200000, 0xc0aac80c, ROMType::SOUND_SAMPLE_SIMM, 0 },
    { "pga-simm.05b",  0x200000, 0x37a65d86, ROMType::SOUND_SAMPLE_SIMM, 0 },
    { "pga-simm.06a",  0x200000, 0xd3f1e934, ROMType::SOUND_SAMPLE_SIMM, 0 },
    { "pga-simm.06b",  0x200000, 0x8b39489a, ROMType::SOUND_SAMPLE_SIMM, 0 },
    { "progear.key",   0x000014, 0xeee6b2a8, ROMType::ENCRYPTION_KEY, 0 },
};

// Street Fighter Alpha: Warriors' Dreams
static const ROMEntry sfa_roms[] = {
    { "sfze.03d",      0x080000, 0xebf2054d, ROMType::PROGRAM, 0 },
    { "sfz.04b",       0x080000, 0x8b73b0e5, ROMType::PROGRAM, 0 },
    { "sfz.05a",       0x080000, 0x0810544d, ROMType::PROGRAM, 0 },
    { "sfz.06",        0x080000, 0x806e8f38, ROMType::PROGRAM, 0 },
    { "sfz.14m",       0x200000, 0x90fefdb3, ROMType::GRAPHICS, 0 },
    { "sfz.16m",       0x200000, 0x5354c948, ROMType::GRAPHICS, 0 },
    { "sfz.18m",       0x200000, 0x41a1e790, ROMType::GRAPHICS, 0 },
    { "sfz.20m",       0x200000, 0xa549df98, ROMType::GRAPHICS, 0 },
    { "sfz.01",        0x020000, 0xffffec7d, ROMType::SOUND_PROGRAM, 0 },
    { "sfz.02",        0x020000, 0x45f46a08, ROMType::SOUND_PROGRAM, 0 },
    { "sfz.11m",       0x200000, 0xc4b093cd, ROMType::SOUND_SAMPLE, 0 },
    { "sfz.12m",       0x200000, 0x8bdbc4b4, ROMType::SOUND_SAMPLE, 0 },
    { "sfa.key",       0x000014, 0x7c095631, ROMType::ENCRYPTION_KEY, 0 },
};

// Street Fighter Alpha 2
static const ROMEntry sfa2_roms[] = {
    { "sz2e.03",       0x080000, 0x1061e6bb, ROMType::PROGRAM, 0 },
    { "sz2e.04",       0x080000, 0x22d17b26, ROMType::PROGRAM, 0 },
    { "sz2.05",        0x080000, 0x4b442a7c, ROMType::PROGRAM, 0 },
    { "sz2.06",        0x080000, 0x5b1d49c0, ROMType::PROGRAM, 0 },
    { "sz2.07",        0x080000, 0x8e184246, ROMType::PROGRAM, 0 },
    { "sz2.08",        0x080000, 0x0fe8585d, ROMType::PROGRAM, 0 },
    { "sz2.13m",       0x400000, 0x4d1f1f22, ROMType::GRAPHICS, 0 },
    { "sz2.15m",       0x400000, 0x19cea680, ROMType::GRAPHICS, 0 },
    { "sz2.17m",       0x400000, 0xe01b4588, ROMType::GRAPHICS, 0 },
    { "sz2.19m",       0x400000, 0x0feeda64, ROMType::GRAPHICS, 0 },
    { "sz2.14m",       0x100000, 0x0560c6aa, ROMType::GRAPHICS, 0 },
    { "sz2.16m",       0x100000, 0xae940f87, ROMType::GRAPHICS, 0 },
    { "sz2.18m",       0x100000, 0x4bc3c8bc, ROMType::GRAPHICS, 0 },
    { "sz2.20m",       0x100000, 0x39e674c0, ROMType::GRAPHICS, 0 },
    { "sz2.01a",       0x020000, 0x1bc323cf, ROMType::SOUND_PROGRAM, 0 },
    { "sz2.02a",       0x020000, 0xba6a5013, ROMType::SOUND_PROGRAM, 0 },
    { "sz2.11m",       0x200000, 0xaa47a601, ROMType::SOUND_SAMPLE, 0 },
    { "sz2.12m",       0x200000, 0x2237bc53, ROMType::SOUND_SAMPLE, 0 },
    { "sfa2.key",      0x000014, 0x1578dcb0, ROMType::ENCRYPTION_KEY, 0 },
};

// Street Fighter Alpha 3
static const ROMEntry sfa3_roms[] = {
    { "sz3e.03c",      0x080000, 0x9762b206, ROMType::PROGRAM, 0 },
    { "sz3e.04c",      0x080000, 0x5ad3f721, ROMType::PROGRAM, 0 },
    { "sz3.05c",       0x080000, 0x57fd0a40, ROMType::PROGRAM, 0 },
    { "sz3.06c",       0x080000, 0xf6305f8b, ROMType::PROGRAM, 0 },
    { "sz3.07c",       0x080000, 0x6eab0f6f, ROMType::PROGRAM, 0 },
    { "sz3.08c",       0x080000, 0x910c4a3b, ROMType::PROGRAM, 0 },
    { "sz3.09c",       0x080000, 0xb29e5199, ROMType::PROGRAM, 0 },
    { "sz3.10b",       0x080000, 0xdeb2ff52, ROMType::PROGRAM, 0 },
    { "sz3.13m",       0x400000, 0x0f7a60d9, ROMType::GRAPHICS, 0 },
    { "sz3.15m",       0x400000, 0x8e933741, ROMType::GRAPHICS, 0 },
    { "sz3.17m",       0x400000, 0xd6e98147, ROMType::GRAPHICS, 0 },
    { "sz3.19m",       0x400000, 0xf31a728a, ROMType::GRAPHICS, 0 },
    { "sz3.14m",       0x400000, 0x5ff98297, ROMType::GRAPHICS, 0 },
    { "sz3.16m",       0x400000, 0x52b5bdee, ROMType::GRAPHICS, 0 },
    { "sz3.18m",       0x400000, 0x40631ed5, ROMType::GRAPHICS, 0 },
    { "sz3.20m",       0x400000, 0x763409b4, ROMType::GRAPHICS, 0 },
    { "sz3.01",        0x020000, 0xde810084, ROMType::SOUND_PROGRAM, 0 },
    { "sz3.02",        0x020000, 0x72445dc4, ROMType::SOUND_PROGRAM, 0 },
    { "sz3.11m",       0x400000, 0x1c89eed1, ROMType::SOUND_SAMPLE, 0 },
    { "sz3.12m",       0x400000, 0xf392b13a, ROMType::SOUND_SAMPLE, 0 },
    { "sfa3.key",      0x000014, 0x54fa39c6, ROMType::ENCRYPTION_KEY, 0 },
};

// Super Puzzle Fighter II Turbo
static const ROMEntry spf2t_roms[] = {
    { "pzfe.03",       0x080000, 0x2af51954, ROMType::PROGRAM, 0 },
    { "pzf.04",        0x080000, 0xb80649e2, ROMType::PROGRAM, 0 },
    { "pzf.14m",       0x100000, 0x2d4881cb, ROMType::GRAPHICS, 0 },
    { "pzf.16m",       0x100000, 0x4b0fd1be, ROMType::GRAPHICS, 0 },
    { "pzf.18m",       0x100000, 0xe43aac33, ROMType::GRAPHICS, 0 },
    { "pzf.20m",       0x100000, 0x7f536ff1, ROMType::GRAPHICS, 0 },
    { "pzf.01",        0x020000, 0x600fb2a3, ROMType::SOUND_PROGRAM, 0 },
    { "pzf.02",        0x020000, 0x496076e0, ROMType::SOUND_PROGRAM, 0 },
    { "pzf.11m",       0x200000, 0x78442743, ROMType::SOUND_SAMPLE, 0 },
    { "pzf.12m",       0x200000, 0x399d2c7b, ROMType::SOUND_SAMPLE, 0 },
    { "spf2t.key",     0x000014, 0x4c4dc7e3, ROMType::ENCRYPTION_KEY, 0 },
};

// Super Street Fighter II: The New Challengers
static const ROMEntry ssf2_roms[] = {
    { "ssfe-03b",      0x080000, 0xaf654792, ROMType::PROGRAM, 0 },
    { "ssfe.04",       0x080000, 0xb082aa67, ROMType::PROGRAM, 0 },
    { "ssfe.05",       0x080000, 0x02b9c137, ROMType::PROGRAM, 0 },
    { "ssfe-06b",      0x080000, 0x1c8e44a8, ROMType::PROGRAM, 0 },
    { "ssfe.07",       0x080000, 0x2409001d, ROMType::PROGRAM, 0 },
    { "ssf.13m",       0x200000, 0xcf94d275, ROMType::GRAPHICS, 0 },
    { "ssf.15m",       0x200000, 0x5eb703af, ROMType::GRAPHICS, 0 },
    { "ssf.17m",       0x200000, 0xffa60e0f, ROMType::GRAPHICS, 0 },
    { "ssf.19m",       0x200000, 0x34e825c5, ROMType::GRAPHICS, 0 },
    { "ssf.14m",       0x100000, 0xb7cc32e7, ROMType::GRAPHICS, 0 },
    { "ssf.16m",       0x100000, 0x8376ad18, ROMType::GRAPHICS, 0 },
    { "ssf.18m",       0x100000, 0xf5b1b336, ROMType::GRAPHICS, 0 },
    { "ssf.20m",       0x100000, 0x459d5c6b, ROMType::GRAPHICS, 0 },
    { "ssf-01a",       0x020000, 0x71fcdfc9, ROMType::SOUND_PROGRAM, 0 },
    { "ssf.q01",       0x080000, 0xa6f9da5c, ROMType::SOUND_SAMPLE, 0 },
    { "ssf.q02",       0x080000, 0x8c66ae26, ROMType::SOUND_SAMPLE, 0 },
    { "ssf.q03",       0x080000, 0x695cc2ca, ROMType::SOUND_SAMPLE, 0 },
    { "ssf.q04",       0x080000, 0x9d9ebe32, ROMType::SOUND_SAMPLE, 0 },
    { "ssf.q05",       0x080000, 0x4770e7b7, ROMType::SOUND_SAMPLE, 0 },
    { "ssf.q06",       0x080000, 0x4e79c951, ROMType::SOUND_SAMPLE, 0 },
    { "ssf.q07",       0x080000, 0xcdd14313, ROMType::SOUND_SAMPLE, 0 },
    { "ssf.q08",       0x080000, 0x6f5a088c, ROMType::SOUND_SAMPLE, 0 },
    { "ssf2.key",      0x000014, 0xe469ccbb, ROMType::ENCRYPTION_KEY, 0 },
};

// Super Street Fighter II Turbo
static const ROMEntry ssf2t_roms[] = {
    { "sfxe.03c",      0x080000, 0x2fa1f396, ROMType::PROGRAM, 0 },
    { "sfxe.04a",      0x080000, 0xd0bc29c6, ROMType::PROGRAM, 0 },
    { "sfxe.05",       0x080000, 0x65222964, ROMType::PROGRAM, 0 },
    { "sfxe.06a",      0x080000, 0x8fe9f531, ROMType::PROGRAM, 0 },
    { "sfxe.07",       0x080000, 0x8a7d0cb6, ROMType::PROGRAM, 0 },
    { "sfxe.08",       0x080000, 0x74c24062, ROMType::PROGRAM, 0 },
    { "sfx.09",        0x080000, 0x642fae3f, ROMType::PROGRAM, 0 },
    { "sfx.13m",       0x200000, 0xcf94d275, ROMType::GRAPHICS, 0 },
    { "sfx.15m",       0x200000, 0x5eb703af, ROMType::GRAPHICS, 0 },
    { "sfx.17m",       0x200000, 0xffa60e0f, ROMType::GRAPHICS, 0 },
    { "sfx.19m",       0x200000, 0x34e825c5, ROMType::GRAPHICS, 0 },
    { "sfx.14m",       0x100000, 0xb7cc32e7, ROMType::GRAPHICS, 0 },
    { "sfx.16m",       0x100000, 0x8376ad18, ROMType::GRAPHICS, 0 },
    { "sfx.18m",       0x100000, 0xf5b1b336, ROMType::GRAPHICS, 0 },
    { "sfx.20m",       0x100000, 0x459d5c6b, ROMType::GRAPHICS, 0 },
    { "sfx.21m",       0x100000, 0xe32854af, ROMType::GRAPHICS, 0 },
    { "sfx.23m",       0x100000, 0x760f2927, ROMType::GRAPHICS, 0 },
    { "sfx.25m",       0x100000, 0x1ee90208, ROMType::GRAPHICS, 0 },
    { "sfx.27m",       0x100000, 0xf814400f, ROMType::GRAPHICS, 0 },
    { "sfx.01",        0x020000, 0xb47b8835, ROMType::SOUND_PROGRAM, 0 },
    { "sfx.02",        0x020000, 0x0022633f, ROMType::SOUND_PROGRAM, 0 },
    { "sfx.11m",       0x200000, 0x9bdbd476, ROMType::SOUND_SAMPLE, 0 },
    { "sfx.12m",       0x200000, 0xa05e3aab, ROMType::SOUND_SAMPLE, 0 },
    { "ssf2t.key",     0x000014, 0x524d608e, ROMType::ENCRYPTION_KEY, 0 },
};

// Vampire Savior: The Lord of Vampire
static const ROMEntry vsav_roms[] = {
    { "vm3e.03d",      0x080000, 0xf5962a8c, ROMType::PROGRAM, 0 },
    { "vm3e.04d",      0x080000, 0x21b40ea2, ROMType::PROGRAM, 0 },
    { "vm3.05a",       0x080000, 0x4118e00f, ROMType::PROGRAM, 0 },
    { "vm3.06a",       0x080000, 0x2f4fd3a9, ROMType::PROGRAM, 0 },
    { "vm3.07b",       0x080000, 0xcbda91b8, ROMType::PROGRAM, 0 },
    { "vm3.08a",       0x080000, 0x6ca47259, ROMType::PROGRAM, 0 },
    { "vm3.09b",       0x080000, 0xf4a339e3, ROMType::PROGRAM, 0 },
    { "vm3.10b",       0x080000, 0xfffbb5b8, ROMType::PROGRAM, 0 },
    { "vm3.13m",       0x400000, 0xfd8a11eb, ROMType::GRAPHICS, 0 },
    { "vm3.15m",       0x400000, 0xdd1e7d4e, ROMType::GRAPHICS, 0 },
    { "vm3.17m",       0x400000, 0x6b89445e, ROMType::GRAPHICS, 0 },
    { "vm3.19m",       0x400000, 0x3830fdc7, ROMType::GRAPHICS, 0 },
    { "vm3.14m",       0x400000, 0xc1a28e6c, ROMType::GRAPHICS, 0 },
    { "vm3.16m",       0x400000, 0x194a7304, ROMType::GRAPHICS, 0 },
    { "vm3.18m",       0x400000, 0xdf9a9f47, ROMType::GRAPHICS, 0 },
    { "vm3.20m",       0x400000, 0xc22fc3d9, ROMType::GRAPHICS, 0 },
    { "vm3.01",        0x020000, 0xf778769b, ROMType::SOUND_PROGRAM, 0 },
    { "vm3.02",        0x020000, 0xcc09faa1, ROMType::SOUND_PROGRAM, 0 },
    { "vm3.11m",       0x400000, 0xe80e956e, ROMType::SOUND_SAMPLE, 0 },
    { "vm3.12m",       0x400000, 0x9cd71557, ROMType::SOUND_SAMPLE, 0 },
    { "vsav.key",      0x000014, 0xa6e3b164, ROMType::ENCRYPTION_KEY, 0 },
};

// Vampire Savior 2: The Lord of Vampire
static const ROMEntry vsav2_roms[] = {
    { "vs2j.03",       0x080000, 0x89fd86b4, ROMType::PROGRAM, 0 },
    { "vs2j.04",       0x080000, 0x107c091b, ROMType::PROGRAM, 0 },
    { "vs2j.05",       0x080000, 0x61979638, ROMType::PROGRAM, 0 },
    { "vs2j.06",       0x080000, 0xf37c5bc2, ROMType::PROGRAM, 0 },
    { "vs2j.07",       0x080000, 0x8f885809, ROMType::PROGRAM, 0 },
    { "vs2j.08",       0x080000, 0x2018c120, ROMType::PROGRAM, 0 },
    { "vs2j.09",       0x080000, 0xfac3c217, ROMType::PROGRAM, 0 },
    { "vs2j.10",       0x080000, 0xeb490213, ROMType::PROGRAM, 0 },
    { "vs2.13m",       0x400000, 0x5c852f52, ROMType::GRAPHICS, 0 },
    { "vs2.15m",       0x400000, 0xa20f58af, ROMType::GRAPHICS, 0 },
    { "vs2.17m",       0x400000, 0x39db59ad, ROMType::GRAPHICS, 0 },
    { "vs2.19m",       0x400000, 0x00c763a7, ROMType::GRAPHICS, 0 },
    { "vs2.14m",       0x400000, 0xcd09bd63, ROMType::GRAPHICS, 0 },
    { "vs2.16m",       0x400000, 0xe0182c15, ROMType::GRAPHICS, 0 },
    { "vs2.18m",       0x400000, 0x778dc4f6, ROMType::GRAPHICS, 0 },
    { "vs2.20m",       0x400000, 0x605d9d1d, ROMType::GRAPHICS, 0 },
    { "vs2.01",        0x020000, 0x35190139, ROMType::SOUND_PROGRAM, 0 },
    { "vs2.02",        0x020000, 0xc32dba09, ROMType::SOUND_PROGRAM, 0 },
    { "vs2.11m",       0x400000, 0xd67e47b7, ROMType::SOUND_SAMPLE, 0 },
    { "vs2.12m",       0x400000, 0x6d020a14, ROMType::SOUND_SAMPLE, 0 },
    { "vsav2.key",     0x000014, 0x289028ce, ROMType::ENCRYPTION_KEY, 0 },
};

// X-Men: Children of the Atom
static const ROMEntry xmcota_roms[] = {
    { "xmne.03f",      0x080000, 0x5a726d13, ROMType::PROGRAM, 0 },
    { "xmne.04f",      0x080000, 0x06a83f3a, ROMType::PROGRAM, 0 },
    { "xmne.05b",      0x080000, 0x87b0ed0f, ROMType::PROGRAM, 0 },
    { "xmn.06a",       0x080000, 0x1b86a328, ROMType::PROGRAM, 0 },
    { "xmn.07a",       0x080000, 0x2c142a44, ROMType::PROGRAM, 0 },
    { "xmn.08a",       0x080000, 0xf712d44f, ROMType::PROGRAM, 0 },
    { "xmn.09a",       0x080000, 0x9241cae8, ROMType::PROGRAM, 0 },
    { "xmne.10b",      0x080000, 0xcb36b0a4, ROMType::PROGRAM, 0 },
    { "xmn.13m",       0x400000, 0xbf4df073, ROMType::GRAPHICS, 0 },
    { "xmn.15m",       0x400000, 0x4d7e4cef, ROMType::GRAPHICS, 0 },
    { "xmn.17m",       0x400000, 0x513eea17, ROMType::GRAPHICS, 0 },
    { "xmn.19m",       0x400000, 0xd23897fc, ROMType::GRAPHICS, 0 },
    { "xmn.14m",       0x400000, 0x778237b7, ROMType::GRAPHICS, 0 },
    { "xmn.16m",       0x400000, 0x67b36948, ROMType::GRAPHICS, 0 },
    { "xmn.18m",       0x400000, 0x015a7c4c, ROMType::GRAPHICS, 0 },
    { "xmn.20m",       0x400000, 0x9dde2758, ROMType::GRAPHICS, 0 },
    { "xmn.01a",       0x020000, 0x40f479ea, ROMType::SOUND_PROGRAM, 0 },
    { "xmn.02a",       0x020000, 0x39d9b5ad, ROMType::SOUND_PROGRAM, 0 },
    { "xmn.11m",       0x200000, 0xc848a6bc, ROMType::SOUND_SAMPLE, 0 },
    { "xmn.12m",       0x200000, 0x729c188f, ROMType::SOUND_SAMPLE, 0 },
    { "xmcota.key",    0x000014, 0x6665bbfb, ROMType::ENCRYPTION_KEY, 0 },
};

// X-Men vs. Street Fighter
static const ROMEntry xmvsf_roms[] = {
    { "xvse.03f",      0x080000, 0xdb06413f, ROMType::PROGRAM, 0 },
    { "xvse.04f",      0x080000, 0xef015aef, ROMType::PROGRAM, 0 },
    { "xvs.05a",       0x080000, 0x7db6025d, ROMType::PROGRAM, 0 },
    { "xvs.06a",       0x080000, 0xe8e2c75c, ROMType::PROGRAM, 0 },
    { "xvs.07",        0x080000, 0x08f0abed, ROMType::PROGRAM, 0 },
    { "xvs.08",        0x080000, 0x81929675, ROMType::PROGRAM, 0 },
    { "xvs.09",        0x080000, 0x9641f36b, ROMType::PROGRAM, 0 },
    { "xvs.13m",       0x400000, 0xf6684efd, ROMType::GRAPHICS, 0 },
    { "xvs.15m",       0x400000, 0x29109221, ROMType::GRAPHICS, 0 },
    { "xvs.17m",       0x400000, 0x92db3474, ROMType::GRAPHICS, 0 },
    { "xvs.19m",       0x400000, 0x3733473c, ROMType::GRAPHICS, 0 },
    { "xvs.14m",       0x400000, 0xbcac2e41, ROMType::GRAPHICS, 0 },
    { "xvs.16m",       0x400000, 0xea04a272, ROMType::GRAPHICS, 0 },
    { "xvs.18m",       0x400000, 0xb0def86a, ROMType::GRAPHICS, 0 },
    { "xvs.20m",       0x400000, 0x4b40ff9f, ROMType::GRAPHICS, 0 },
    { "xvs.01",        0x020000, 0x3999e93a, ROMType::SOUND_PROGRAM, 0 },
    { "xvs.02",        0x020000, 0x101bdee9, ROMType::SOUND_PROGRAM, 0 },
    { "xvs.11m",       0x200000, 0x9cadcdbc, ROMType::SOUND_SAMPLE, 0 },
    { "xvs.12m",       0x200000, 0x7b11e460, ROMType::SOUND_SAMPLE, 0 },
    { "xmvsf.key",     0x000014, 0xd5c07311, ROMType::ENCRYPTION_KEY, 0 },
};

// Armored Warriors
static const ROMEntry armwar_roms[] = {
    { "pwge.03c",      0x080000, 0x31f74931, ROMType::PROGRAM, 0 },
    { "pwge.04c",      0x080000, 0x16f34f5f, ROMType::PROGRAM, 0 },
    { "pwge.05b",      0x080000, 0x4403ed08, ROMType::PROGRAM, 0 },
    { "pwg.06",        0x080000, 0x87a60ce8, ROMType::PROGRAM, 0 },
    { "pwg.07",        0x080000, 0xf7b148df, ROMType::PROGRAM, 0 },
    { "pwg.08",        0x080000, 0xcc62823e, ROMType::PROGRAM, 0 },
    { "pwg.09a",       0x080000, 0x4c26baee, ROMType::PROGRAM, 0 },
    { "pwg.10",        0x080000, 0x07c4fb28, ROMType::PROGRAM, 0 },
    { "pwg.13m",       0x400000, 0xae8fe08e, ROMType::GRAPHICS, 0 },
    { "pwg.15m",       0x400000, 0xdb560f58, ROMType::GRAPHICS, 0 },
    { "pwg.17m",       0x400000, 0xbc475b94, ROMType::GRAPHICS, 0 },
    { "pwg.19m",       0x400000, 0x07439ff7, ROMType::GRAPHICS, 0 },
    { "pwg.14m",       0x100000, 0xc3f9ba63, ROMType::GRAPHICS, 0 },
    { "pwg.16m",       0x100000, 0x815b0e7b, ROMType::GRAPHICS, 0 },
    { "pwg.18m",       0x100000, 0x0109c71b, ROMType::GRAPHICS, 0 },
    { "pwg.20m",       0x100000, 0xeb75ffbe, ROMType::GRAPHICS, 0 },
    { "pwg.01",        0x020000, 0x18a5c0e4, ROMType::SOUND_PROGRAM, 0 },
    { "pwg.02",        0x020000, 0xc9dfffa6, ROMType::SOUND_PROGRAM, 0 },
    { "pwg.11m",       0x200000, 0xa78f7433, ROMType::SOUND_SAMPLE, 0 },
    { "pwg.12m",       0x200000, 0x77438ed0, ROMType::SOUND_SAMPLE, 0 },
    { "armwar.key",    0x000014, 0xfe979382, ROMType::ENCRYPTION_KEY, 0 },
};

// Battle Circuit
static const ROMEntry batcir_roms[] = {
    { "btce.03",       0x080000, 0xbc60484b, ROMType::PROGRAM, 0 },
    { "btce.04",       0x080000, 0x457d55f6, ROMType::PROGRAM, 0 },
    { "btce.05",       0x080000, 0xe86560d7, ROMType::PROGRAM, 0 },
    { "btce.06",       0x080000, 0xf778e61b, ROMType::PROGRAM, 0 },
    { "btc.07",        0x080000, 0x7322d5db, ROMType::PROGRAM, 0 },
    { "btc.08",        0x080000, 0x6aac85ab, ROMType::PROGRAM, 0 },
    { "btc.09",        0x080000, 0x1203db08, ROMType::PROGRAM, 0 },
    { "btc.13m",       0x400000, 0xdc705bad, ROMType::GRAPHICS, 0 },
    { "btc.15m",       0x400000, 0xe5779a3c, ROMType::GRAPHICS, 0 },
    { "btc.17m",       0x400000, 0xb33f4112, ROMType::GRAPHICS, 0 },
    { "btc.19m",       0x400000, 0xa6fcdb7e, ROMType::GRAPHICS, 0 },
    { "btc.01",        0x020000, 0x1e194310, ROMType::SOUND_PROGRAM, 0 },
    { "btc.02",        0x020000, 0x01aeb8e6, ROMType::SOUND_PROGRAM, 0 },
    { "btc.11m",       0x200000, 0xc27f2229, ROMType::SOUND_SAMPLE, 0 },
    { "btc.12m",       0x200000, 0x418a2e33, ROMType::SOUND_SAMPLE, 0 },
    { "batcir.key",    0x000014, 0xe316ae67, ROMType::ENCRYPTION_KEY, 0 },
};

// Janpai Puzzle Choukou
static const ROMEntry choko_roms[] = {
    { "tkoj.03",       0x080000, 0x11f5452f, ROMType::PROGRAM, 0 },
    { "tkoj.04",       0x080000, 0x68655378, ROMType::PROGRAM, 0 },
    { "tkoj1_d.simm1", 0x200000, 0x6933377d, ROMType::GRAPHICS_SIMM, 0 },
    { "tkoj1_c.simm1", 0x200000, 0x7f668950, ROMType::GRAPHICS_SIMM, 0 },
    { "tkoj1_b.simm1", 0x200000, 0xcfb68ca9, ROMType::GRAPHICS_SIMM, 0 },
    { "tkoj1_a.simm1", 0x200000, 0x437e21c5, ROMType::GRAPHICS_SIMM, 0 },
    { "tkoj3_d.simm3", 0x200000, 0xa9e32b57, ROMType::GRAPHICS_SIMM, 0 },
    { "tkoj3_c.simm3", 0x200000, 0xb7ab9338, ROMType::GRAPHICS_SIMM, 0 },
    { "tkoj3_b.simm3", 0x200000, 0x4d3f919a, ROMType::GRAPHICS_SIMM, 0 },
    { "tkoj3_a.simm3", 0x200000, 0xcfef17ab, ROMType::GRAPHICS_SIMM, 0 },
    { "tko.01",        0x020000, 0x6eda50c2, ROMType::SOUND_PROGRAM, 0 },
    { "tkoj5_a.simm5", 0x200000, 0xab45d509, ROMType::SOUND_SAMPLE_SIMM_BYTESWAP, 0 },
    { "tkoj5_b.simm5", 0x200000, 0xfa905c3d, ROMType::SOUND_SAMPLE_SIMM_BYTESWAP, 0 },
    { "choko.key",     0x000014, 0x08505e8b, ROMType::ENCRYPTION_KEY, 0 },
};

// Capcom Sports Club
static const ROMEntry csclub_roms[] = {
    { "csce.03a",      0x080000, 0x824082be, ROMType::PROGRAM, 0 },
    { "csce.04a",      0x080000, 0x74e6a4fe, ROMType::PROGRAM, 0 },
    { "csce.05a",      0x080000, 0x8ae0df19, ROMType::PROGRAM, 0 },
    { "csce.06a",      0x080000, 0x51f2f0d3, ROMType::PROGRAM, 0 },
    { "csce.07a",      0x080000, 0x003968fd, ROMType::PROGRAM, 0 },
    { "csc.73",        0x080000, 0x335f07c3, ROMType::GRAPHICS_SPLIT4, 0 },
    { "csc.74",        0x080000, 0xab215357, ROMType::GRAPHICS_SPLIT4, 0 },
    { "csc.75",        0x080000, 0xa2367381, ROMType::GRAPHICS_SPLIT4, 0 },
    { "csc.76",        0x080000, 0x728aac1f, ROMType::GRAPHICS_SPLIT4, 0 },
    { "csc.63",        0x080000, 0x3711b8ca, ROMType::GRAPHICS_SPLIT4, 0 },
    { "csc.64",        0x080000, 0x828a06d8, ROMType::GRAPHICS_SPLIT4, 0 },
    { "csc.65",        0x080000, 0x86ee4569, ROMType::GRAPHICS_SPLIT4, 0 },
    { "csc.66",        0x080000, 0xc24f577f, ROMType::GRAPHICS_SPLIT4, 0 },
    { "csc.83",        0x080000, 0x0750d12a, ROMType::GRAPHICS_SPLIT4, 0 },
    { "csc.84",        0x080000, 0x90a92f39, ROMType::GRAPHICS_SPLIT4, 0 },
    { "csc.85",        0x080000, 0xd08ab012, ROMType::GRAPHICS_SPLIT4, 0 },
    { "csc.86",        0x080000, 0x41652583, ROMType::GRAPHICS_SPLIT4, 0 },
    { "csc.93",        0x080000, 0xa756c7f7, ROMType::GRAPHICS_SPLIT4, 0 },
    { "csc.94",        0x080000, 0xfb7ccc73, ROMType::GRAPHICS_SPLIT4, 0 },
    { "csc.95",        0x080000, 0x4d014297, ROMType::GRAPHICS_SPLIT4, 0 },
    { "csc.96",        0x080000, 0x6754b1ef, ROMType::GRAPHICS_SPLIT4, 0 },
    { "csc.01",        0x020000, 0xee162111, ROMType::SOUND_PROGRAM, 0 },
    { "csc.51",        0x080000, 0x5a52afd5, ROMType::SOUND_SAMPLE, 0 },
    { "csc.52",        0x080000, 0x1408a811, ROMType::SOUND_SAMPLE, 0 },
    { "csc.53",        0x080000, 0x4fb9f57c, ROMType::SOUND_SAMPLE, 0 },
    { "csc.54",        0x080000, 0x9a8f40ec, ROMType::SOUND_SAMPLE, 0 },
    { "csc.55",        0x080000, 0x91529a91, ROMType::SOUND_SAMPLE, 0 },
    { "csc.56",        0x080000, 0x9a345334, ROMType::SOUND_SAMPLE, 0 },
    { "csc.57",        0x080000, 0xaedc27f2, ROMType::SOUND_SAMPLE, 0 },
    { "csc.58",        0x080000, 0x2300b7b3, ROMType::SOUND_SAMPLE, 0 },
    { "csclub.key",    0x000014, 0x903907d7, ROMType::ENCRYPTION_KEY, 0 },
};

// Cyberbots: Fullmetal Madness
static const ROMEntry cybots_roms[] = {
    { "cybe.03",       0x080000, 0x234381cd, ROMType::PROGRAM, 0 },
    { "cybe.04",       0x080000, 0x80691061, ROMType::PROGRAM, 0 },
    { "cyb.05",        0x080000, 0xec40408e, ROMType::PROGRAM, 0 },
    { "cyb.06",        0x080000, 0x1ad0bed2, ROMType::PROGRAM, 0 },
    { "cyb.07",        0x080000, 0x6245a39a, ROMType::PROGRAM, 0 },
    { "cyb.08",        0x080000, 0x4b48e223, ROMType::PROGRAM, 0 },
    { "cyb.09",        0x080000, 0xe15238f6, ROMType::PROGRAM, 0 },
    { "cyb.10",        0x080000, 0x75f4003b, ROMType::PROGRAM, 0 },
    { "cyb.13m",       0x400000, 0xf0dce192, ROMType::GRAPHICS, 0 },
    { "cyb.15m",       0x400000, 0x187aa39c, ROMType::GRAPHICS, 0 },
    { "cyb.17m",       0x400000, 0x8a0e4b12, ROMType::GRAPHICS, 0 },
    { "cyb.19m",       0x400000, 0x34b62612, ROMType::GRAPHICS, 0 },
    { "cyb.14m",       0x400000, 0xc1537957, ROMType::GRAPHICS, 0 },
    { "cyb.16m",       0x400000, 0x15349e86, ROMType::GRAPHICS, 0 },
    { "cyb.18m",       0x400000, 0xd83e977d, ROMType::GRAPHICS, 0 },
    { "cyb.20m",       0x400000, 0x77cdad5c, ROMType::GRAPHICS, 0 },
    { "cyb.01",        0x020000, 0x9c0fb079, ROMType::SOUND_PROGRAM, 0 },
    { "cyb.02",        0x020000, 0x51cb0c4e, ROMType::SOUND_PROGRAM, 0 },
    { "cyb.11m",       0x200000, 0x362ccab2, ROMType::SOUND_SAMPLE, 0 },
    { "cyb.12m",       0x200000, 0x7066e9cc, ROMType::SOUND_SAMPLE, 0 },
    { "cybots.key",    0x000014, 0x9bbcbef3, ROMType::ENCRYPTION_KEY, 0 },
};

// Dimahoo
static const ROMEntry dimahoo_roms[] = {
    { "gmde.03",       0x080000, 0x968fcecd, ROMType::PROGRAM, 0 },
    { "gmd.04",        0x080000, 0x37485567, ROMType::PROGRAM, 0 },
    { "gmd.05",        0x080000, 0xda269ffb, ROMType::PROGRAM, 0 },
    { "gmd.06",        0x080000, 0x55b483c9, ROMType::PROGRAM, 0 },
    { "gmd.13m",       0x400000, 0x80dd19f0, ROMType::GRAPHICS, 0 },
    { "gmd.15m",       0x400000, 0xdfd93a78, ROMType::GRAPHICS, 0 },
    { "gmd.17m",       0x400000, 0x16356520, ROMType::GRAPHICS, 0 },
    { "gmd.19m",       0x400000, 0xdfc33031, ROMType::GRAPHICS, 0 },
    { "gmd.01",        0x020000, 0x3f9bc985, ROMType::SOUND_PROGRAM, 0 },
    { "gmd.02",        0x020000, 0x3fd39dde, ROMType::SOUND_PROGRAM, 0 },
    { "gmd.11m",       0x400000, 0x06a65542, ROMType::SOUND_SAMPLE, 0 },
    { "gmd.12m",       0x400000, 0x50bc7a31, ROMType::SOUND_SAMPLE, 0 },
    { "dimahoo.key",   0x000014, 0x7d6d2db9, ROMType::ENCRYPTION_KEY, 0 },
};

// Darkstalkers: The Night Warriors
static const ROMEntry dstlk_roms[] = {
    { "vame.03a",      0x080000, 0x004c9cff, ROMType::PROGRAM, 0 },
    { "vame.04a",      0x080000, 0xae413ff2, ROMType::PROGRAM, 0 },
    { "vame.05a",      0x080000, 0x60678756, ROMType::PROGRAM, 0 },
    { "vame.06a",      0x080000, 0x912870b3, ROMType::PROGRAM, 0 },
    { "vame.07a",      0x080000, 0xdabae3e8, ROMType::PROGRAM, 0 },
    { "vame.08a",      0x080000, 0x2c6e3077, ROMType::PROGRAM, 0 },
    { "vame.09a",      0x080000, 0xf16db74b, ROMType::PROGRAM, 0 },
    { "vame.10a",      0x080000, 0x701e2147, ROMType::PROGRAM, 0 },
    { "vam.13m",       0x400000, 0xc51baf99, ROMType::GRAPHICS, 0 },
    { "vam.15m",       0x400000, 0x3ce83c77, ROMType::GRAPHICS, 0 },
    { "vam.17m",       0x400000, 0x4f2408e0, ROMType::GRAPHICS, 0 },
    { "vam.19m",       0x400000, 0x9ff60250, ROMType::GRAPHICS, 0 },
    { "vam.14m",       0x100000, 0xbd87243c, ROMType::GRAPHICS, 0 },
    { "vam.16m",       0x100000, 0xafec855f, ROMType::GRAPHICS, 0 },
    { "vam.18m",       0x100000, 0x3a033625, ROMType::GRAPHICS, 0 },
    { "vam.20m",       0x100000, 0x2bff6a89, ROMType::GRAPHICS, 0 },
    { "vam.01",        0x020000, 0x64b685d5, ROMType::SOUND_PROGRAM, 0 },
    { "vam.02",        0x020000, 0xcf7c97c7, ROMType::SOUND_PROGRAM, 0 },
    { "vam.11m",       0x200000, 0x4a39deb2, ROMType::SOUND_SAMPLE, 0 },
    { "vam.12m",       0x200000, 0x1a3e5c03, ROMType::SOUND_SAMPLE, 0 },
    { "dstlk.key",     0x000014, 0xcfa46dec, ROMType::ENCRYPTION_KEY, 0 },
};

// Eco Fighters
static const ROMEntry ecofghtr_roms[] = {
    { "uece.03",       0x080000, 0xec2c1137, ROMType::PROGRAM, 0 },
    { "uece.04",       0x080000, 0xb35f99db, ROMType::PROGRAM, 0 },
    { "uece.05",       0x080000, 0xd9d42d31, ROMType::PROGRAM, 0 },
    { "uece.06",       0x080000, 0x9d9771cf, ROMType::PROGRAM, 0 },
    { "uec.13m",       0x200000, 0xdcaf1436, ROMType::GRAPHICS, 0 },
    { "uec.15m",       0x200000, 0x2807df41, ROMType::GRAPHICS, 0 },
    { "uec.17m",       0x200000, 0x8a708d02, ROMType::GRAPHICS, 0 },
    { "uec.19m",       0x200000, 0xde7be0ef, ROMType::GRAPHICS, 0 },
    { "uec.14m",       0x100000, 0x1a003558, ROMType::GRAPHICS, 0 },
    { "uec.16m",       0x100000, 0x4ff8a6f9, ROMType::GRAPHICS, 0 },
    { "uec.18m",       0x100000, 0xb167ae12, ROMType::GRAPHICS, 0 },
    { "uec.20m",       0x100000, 0x1064bdc2, ROMType::GRAPHICS, 0 },
    { "uec.01",        0x020000, 0xc235bd15, ROMType::SOUND_PROGRAM, 0 },
    { "uec.11m",       0x200000, 0x81b25d39, ROMType::SOUND_SAMPLE, 0 },
    { "uec.12m",       0x200000, 0x27729e52, ROMType::SOUND_SAMPLE, 0 },
    { "ecofghtr.key",  0x000014, 0x2250fd9e, ROMType::ENCRYPTION_KEY, 0 },
};

// Final Fight: Anniversary Edition
static const ROMEntry ffightaec2_roms[] = {
    { "ff-23m.8h",     0x080000, 0xb598d599, ROMType::PROGRAM, 0 },
    { "ff-22m.7h",     0x080000, 0x3615cfb9, ROMType::PROGRAM, 0 },
    { "ff-5m.7a",      0x400000, 0x3f4028c5, ROMType::GRAPHICS, 0 },
    { "ff-7m.9a",      0x400000, 0xbe3858b0, ROMType::GRAPHICS, 0 },
    { "ff-1m.3a",      0x400000, 0xed622314, ROMType::GRAPHICS, 0 },
    { "ff-3m.5a",      0x400000, 0xd65b53e9, ROMType::GRAPHICS, 0 },
    { "sz3.01",        0x020000, 0x7ee68d38, ROMType::SOUND_PROGRAM, 0 },
    { "sz3.02",        0x020000, 0x72445dc4, ROMType::SOUND_PROGRAM, 0 },
    { "sz3.11m",       0x400000, 0x71af8d5a, ROMType::SOUND_SAMPLE, 0 },
    { "sz3.12m",       0x400000, 0xf392b13a, ROMType::SOUND_SAMPLE, 0 },
    { "phoenix.key",   0x000014, 0x2cf772b0, ROMType::ENCRYPTION_KEY, 0 },
};

// Giga Wing
static const ROMEntry gigawing_roms[] = {
    { "ggwu.03",       0x080000, 0xac725eb2, ROMType::PROGRAM, 0 },
    { "ggwu.04",       0x080000, 0x392f4118, ROMType::PROGRAM, 0 },
    { "ggw.05",        0x080000, 0x3239d642, ROMType::PROGRAM, 0 },
    { "ggw.13m",       0x400000, 0x105530a4, ROMType::GRAPHICS, 0 },
    { "ggw.15m",       0x400000, 0x9e774ab9, ROMType::GRAPHICS, 0 },
    { "ggw.17m",       0x400000, 0x466e0ba4, ROMType::GRAPHICS, 0 },
    { "ggw.19m",       0x400000, 0x840c8dea, ROMType::GRAPHICS, 0 },
    { "ggw.01",        0x020000, 0x4c6351d5, ROMType::SOUND_PROGRAM, 0 },
    { "ggw.11m",       0x400000, 0xe172acf5, ROMType::SOUND_SAMPLE, 0 },
    { "ggw.12m",       0x400000, 0x4bee4e8f, ROMType::SOUND_SAMPLE, 0 },
    { "gigawing.key",  0x000014, 0x5076c26b, ROMType::ENCRYPTION_KEY, 0 },
};

// Jyangokushi: Haoh no Saihai
static const ROMEntry jyangoku_roms[] = {
    { "majj.03",       0x080000, 0x4614a3b2, ROMType::PROGRAM, 0 },
    { "maj1_d.simm1",  0x200000, 0xba0fe27b, ROMType::GRAPHICS_SIMM, 0 },
    { "maj1_c.simm1",  0x200000, 0x2cd141bf, ROMType::GRAPHICS_SIMM, 0 },
    { "maj1_b.simm1",  0x200000, 0xe29e4c26, ROMType::GRAPHICS_SIMM, 0 },
    { "maj1_a.simm1",  0x200000, 0x7f68b88a, ROMType::GRAPHICS_SIMM, 0 },
    { "maj3_d.simm3",  0x200000, 0x3aaeb90b, ROMType::GRAPHICS_SIMM, 0 },
    { "maj3_c.simm3",  0x200000, 0x97894cea, ROMType::GRAPHICS_SIMM, 0 },
    { "maj3_b.simm3",  0x200000, 0xec737d9d, ROMType::GRAPHICS_SIMM, 0 },
    { "maj3_a.simm3",  0x200000, 0xc23b6f22, ROMType::GRAPHICS_SIMM, 0 },
    { "maj.01",        0x020000, 0x1fe8c213, ROMType::SOUND_PROGRAM, 0 },
    { "maj5_a.simm5",  0x200000, 0x5ad9ee53, ROMType::SOUND_SAMPLE_SIMM_BYTESWAP, 0 },
    { "maj5_b.simm5",  0x200000, 0xefb3dbfb, ROMType::SOUND_SAMPLE_SIMM_BYTESWAP, 0 },
    { "jyangoku.key",  0x000014, 0x95b0a560, ROMType::ENCRYPTION_KEY, 0 },
};

// Mega Man 2: The Power Fighters
static const ROMEntry megaman2_roms[] = {
    { "rm2u.03",       0x080000, 0x8ffc2cd1, ROMType::PROGRAM, 0 },
    { "rm2u.04",       0x080000, 0xbb30083a, ROMType::PROGRAM, 0 },
    { "rm2.05",        0x080000, 0x02ee9efc, ROMType::PROGRAM, 0 },
    { "rm2.14m",       0x200000, 0x9b1f00b4, ROMType::GRAPHICS, 0 },
    { "rm2.16m",       0x200000, 0xc2bb0c24, ROMType::GRAPHICS, 0 },
    { "rm2.18m",       0x200000, 0x12257251, ROMType::GRAPHICS, 0 },
    { "rm2.20m",       0x200000, 0xf9b6e786, ROMType::GRAPHICS, 0 },
    { "rm2.01a",       0x020000, 0xd18e7859, ROMType::SOUND_PROGRAM, 0 },
    { "rm2.02",        0x020000, 0xc463ece0, ROMType::SOUND_PROGRAM, 0 },
    { "rm2.11m",       0x200000, 0x2106174d, ROMType::SOUND_SAMPLE, 0 },
    { "rm2.12m",       0x200000, 0x546c1636, ROMType::SOUND_SAMPLE, 0 },
    { "megaman2.key",  0x000014, 0x6828ed6d, ROMType::ENCRYPTION_KEY, 0 },
};

// Mars Matrix: Hyper Solid Shooting
static const ROMEntry mmatrix_roms[] = {
    { "mmxu.03",       0x080000, 0xab65b599, ROMType::PROGRAM, 0 },
    { "mmxu.04",       0x080000, 0x0135fc6c, ROMType::PROGRAM, 0 },
    { "mmxu.05",       0x080000, 0xf1fd2b84, ROMType::PROGRAM, 0 },
    { "mmx.13m",       0x400000, 0x04748718, ROMType::GRAPHICS, 0 },
    { "mmx.15m",       0x400000, 0x38074f44, ROMType::GRAPHICS, 0 },
    { "mmx.17m",       0x400000, 0xe4635e35, ROMType::GRAPHICS, 0 },
    { "mmx.19m",       0x400000, 0x4400a3f2, ROMType::GRAPHICS, 0 },
    { "mmx.14m",       0x400000, 0xd52bf491, ROMType::GRAPHICS, 0 },
    { "mmx.16m",       0x400000, 0x23f70780, ROMType::GRAPHICS, 0 },
    { "mmx.18m",       0x400000, 0x2562c9d5, ROMType::GRAPHICS, 0 },
    { "mmx.20m",       0x400000, 0x583a9687, ROMType::GRAPHICS, 0 },
    { "mmx.01",        0x020000, 0xc57e8171, ROMType::SOUND_PROGRAM, 0 },
    { "mmx.11m",       0x400000, 0x4180b39f, ROMType::SOUND_SAMPLE, 0 },
    { "mmx.12m",       0x400000, 0x95e22a59, ROMType::SOUND_SAMPLE, 0 },
    { "mmatrix.key",   0x000014, 0x8ed66bc4, ROMType::ENCRYPTION_KEY, 0 },
};

// Mighty! Pang
static const ROMEntry mpang_roms[] = {
    { "mpne.03c",      0x080000, 0xfe16fc9f, ROMType::PROGRAM, 0 },
    { "mpne.04c",      0x080000, 0x2cc5ec22, ROMType::PROGRAM, 0 },
    { "mpn-simm.01c",  0x200000, 0x388db66b, ROMType::GRAPHICS_SIMM, 0 },
    { "mpn-simm.01d",  0x200000, 0xaff1b494, ROMType::GRAPHICS_SIMM, 0 },
    { "mpn-simm.01a",  0x200000, 0xa9c4857b, ROMType::GRAPHICS_SIMM, 0 },
    { "mpn-simm.01b",  0x200000, 0xf759df22, ROMType::GRAPHICS_SIMM, 0 },
    { "mpn-simm.03c",  0x200000, 0xdec6b720, ROMType::GRAPHICS_SIMM, 0 },
    { "mpn-simm.03d",  0x200000, 0xf8774c18, ROMType::GRAPHICS_SIMM, 0 },
    { "mpn-simm.03a",  0x200000, 0xc2aea4ec, ROMType::GRAPHICS_SIMM, 0 },
    { "mpn-simm.03b",  0x200000, 0x84d6dc33, ROMType::GRAPHICS_SIMM, 0 },
    { "mpn.01",        0x020000, 0x90c7adb6, ROMType::SOUND_PROGRAM, 0 },
    { "mpn-simm.05a",  0x200000, 0x318a2e21, ROMType::SOUND_SAMPLE_SIMM, 0 },
    { "mpn-simm.05b",  0x200000, 0x5462f4e8, ROMType::SOUND_SAMPLE_SIMM, 0 },
    { "mpang.key",     0x000014, 0x95354b0f, ROMType::ENCRYPTION_KEY, 0 },
};

// Night Warriors: Darkstalkers' Revenge
static const ROMEntry nwarr_roms[] = {
    { "vphe.03f",      0x080000, 0xa922c44f, ROMType::PROGRAM, 0 },
    { "vphe.04c",      0x080000, 0x7312d890, ROMType::PROGRAM, 0 },
    { "vphe.05d",      0x080000, 0xcde8b506, ROMType::PROGRAM, 0 },
    { "vphe.06c",      0x080000, 0xbe99e7d0, ROMType::PROGRAM, 0 },
    { "vphe.07b",      0x080000, 0x69e0e60c, ROMType::PROGRAM, 0 },
    { "vphe.08b",      0x080000, 0xd95a3849, ROMType::PROGRAM, 0 },
    { "vphe.09b",      0x080000, 0x9882561c, ROMType::PROGRAM, 0 },
    { "vphe.10b",      0x080000, 0x976fa62f, ROMType::PROGRAM, 0 },

    { "vph.13m",       0x400000, 0xc51baf99, ROMType::GRAPHICS, 0 },
    { "vph.15m",       0x400000, 0x3ce83c77, ROMType::GRAPHICS, 0 },
    { "vph.17m",       0x400000, 0x4f2408e0, ROMType::GRAPHICS, 0 },
    { "vph.19m",       0x400000, 0x9ff60250, ROMType::GRAPHICS, 0 },
    { "vph.14m",       0x400000, 0x7a0e1add, ROMType::GRAPHICS, 0 },
    { "vph.16m",       0x400000, 0x2f41ca75, ROMType::GRAPHICS, 0 },
    { "vph.18m",       0x400000, 0x64498eed, ROMType::GRAPHICS, 0 },
    { "vph.20m",       0x400000, 0x17f2433f, ROMType::GRAPHICS, 0 },
    { "vph.01",        0x020000, 0x5045dcac, ROMType::SOUND_PROGRAM, 0 },
    { "vph.02",        0x020000, 0x86b60e59, ROMType::SOUND_PROGRAM, 0 },
    { "vph.11m",       0x200000, 0xe1837d33, ROMType::SOUND_SAMPLE, 0 },
    { "vph.12m",       0x200000, 0xfbd3cd90, ROMType::SOUND_SAMPLE, 0 },
    { "nwarr.key",     0x000014, 0x618a13ca, ROMType::ENCRYPTION_KEY, 0 },
};

// Puzz Loop 2
static const ROMEntry pzloop2_roms[] = {
    { "pl2e.03",       0x080000, 0x3b1285b2, ROMType::PROGRAM, 0 },
    { "pl2e.04",       0x080000, 0x40a2d647, ROMType::PROGRAM, 0 },
    { "pl2e.05",       0x080000, 0x0f11d818, ROMType::PROGRAM, 0 },
    { "pl2e.06",       0x080000, 0x86fbbdf4, ROMType::PROGRAM, 0 },
    { "pl2-simm.01c",  0x200000, 0x137b13a7, ROMType::GRAPHICS_SIMM, 0 },
    { "pl2-simm.01d",  0x200000, 0xa2db1507, ROMType::GRAPHICS_SIMM, 0 },
    { "pl2-simm.01a",  0x200000, 0x7e80ff8e, ROMType::GRAPHICS_SIMM, 0 },
    { "pl2-simm.01b",  0x200000, 0xcd93e6ed, ROMType::GRAPHICS_SIMM, 0 },
    { "pl2-simm.03c",  0x200000, 0x0f52bbca, ROMType::GRAPHICS_SIMM, 0 },
    { "pl2-simm.03d",  0x200000, 0xa62712c3, ROMType::GRAPHICS_SIMM, 0 },
    { "pl2-simm.03a",  0x200000, 0xb60c9f8e, ROMType::GRAPHICS_SIMM, 0 },
    { "pl2-simm.03b",  0x200000, 0x83fef284, ROMType::GRAPHICS_SIMM, 0 },
    { "pl2.01",        0x020000, 0x35697569, ROMType::SOUND_PROGRAM, 0 },
    { "pl2-simm.05a",  0x200000, 0x85d8fbe8, ROMType::SOUND_SAMPLE_SIMM, 0 },
    { "pl2-simm.05b",  0x200000, 0x1ed62584, ROMType::SOUND_SAMPLE_SIMM, 0 },
    { "pzloop2.key",   0x000014, 0xae13be78, ROMType::ENCRYPTION_KEY, 0 },
};

// Quiz Nanairo Dreams: Nijiirochou no Kiseki
static const ROMEntry qndream_roms[] = {
    { "tqzj.03a",      0x080000, 0x7acf3e30, ROMType::PROGRAM, 0 },
    { "tqzj.04",       0x080000, 0xf1044a87, ROMType::PROGRAM, 0 },
    { "tqzj.05",       0x080000, 0x4105ba0e, ROMType::PROGRAM, 0 },
    { "tqzj.06",       0x080000, 0xc371e8a5, ROMType::PROGRAM, 0 },
    { "tqz.14m",       0x200000, 0x98af88a2, ROMType::GRAPHICS, 0 },
    { "tqz.16m",       0x200000, 0xdf82d491, ROMType::GRAPHICS, 0 },
    { "tqz.18m",       0x200000, 0x42f132ff, ROMType::GRAPHICS, 0 },
    { "tqz.20m",       0x200000, 0xb2e128a3, ROMType::GRAPHICS, 0 },
    { "tqz.01",        0x020000, 0xe9ce9d0a, ROMType::SOUND_PROGRAM, 0 },
    { "tqz.11m",       0x200000, 0x78e7884f, ROMType::SOUND_SAMPLE, 0 },
    { "tqz.12m",       0x200000, 0x2e049b13, ROMType::SOUND_SAMPLE, 0 },
    { "qndream.key",   0x000014, 0x97eee4ff, ROMType::ENCRYPTION_KEY, 0 },
};

// Ring of Destruction: Slammasters II
static const ROMEntry ringdest_roms[] = {
    { "smbe.03b",      0x080000, 0xb8016278, ROMType::PROGRAM, 0 },
    { "smbe.04b",      0x080000, 0x18c4c447, ROMType::PROGRAM, 0 },
    { "smbe.05b",      0x080000, 0x18ebda7f, ROMType::PROGRAM, 0 },
    { "smbe.06b",      0x080000, 0x89c80007, ROMType::PROGRAM, 0 },
    { "smb.07",        0x080000, 0xb9a11577, ROMType::PROGRAM, 0 },
    { "smb.08",        0x080000, 0xf931b76b, ROMType::PROGRAM, 0 },
    { "smb.13m",       0x200000, 0xd9b2d1de, ROMType::GRAPHICS, 0 },
    { "smb.15m",       0x200000, 0x9a766d92, ROMType::GRAPHICS, 0 },
    { "smb.17m",       0x200000, 0x51800f0f, ROMType::GRAPHICS, 0 },
    { "smb.19m",       0x200000, 0x35757e96, ROMType::GRAPHICS, 0 },
    { "smb.14m",       0x200000, 0xe5bfd0e7, ROMType::GRAPHICS, 0 },
    { "smb.16m",       0x200000, 0xc56c0866, ROMType::GRAPHICS, 0 },
    { "smb.18m",       0x200000, 0x4ded3910, ROMType::GRAPHICS, 0 },
    { "smb.20m",       0x200000, 0x26ea1ec5, ROMType::GRAPHICS, 0 },
    { "smb.21m",       0x080000, 0x0a08c5fc, ROMType::GRAPHICS, 0 },
    { "smb.23m",       0x080000, 0x0911b6c4, ROMType::GRAPHICS, 0 },
    { "smb.25m",       0x080000, 0x82d6c4ec, ROMType::GRAPHICS, 0 },
    { "smb.27m",       0x080000, 0x9b48678b, ROMType::GRAPHICS, 0 },
    { "smb.01",        0x020000, 0x0abc229a, ROMType::SOUND_PROGRAM, 0 },
    { "smb.02",        0x020000, 0xd051679a, ROMType::SOUND_PROGRAM, 0 },
    { "smb.11m",       0x200000, 0xc56935f9, ROMType::SOUND_SAMPLE, 0 },
    { "smb.12m",       0x200000, 0x955b0782, ROMType::SOUND_SAMPLE, 0 },
    { "ringdest.key",  0x000014, 0x17f9269c, ROMType::ENCRYPTION_KEY, 0 },
};

// Street Fighter Zero 2 Alpha
static const ROMEntry sfz2al_roms[] = {
    { "szaa.03",       0x080000, 0x88e7023e, ROMType::PROGRAM, 0 },
    { "szaa.04",       0x080000, 0xae8ec36e, ROMType::PROGRAM, 0 },
    { "szaa.05",       0x080000, 0xf053a55e, ROMType::PROGRAM, 0 },
    { "szaa.06",       0x080000, 0xcfc0e7a8, ROMType::PROGRAM, 0 },
    { "szaa.07",       0x080000, 0x5feb8b20, ROMType::PROGRAM, 0 },
    { "szaa.08",       0x080000, 0x6eb6d412, ROMType::PROGRAM, 0 },
    { "sza.13m",       0x400000, 0x4d1f1f22, ROMType::GRAPHICS, 0 },
    { "sza.15m",       0x400000, 0x19cea680, ROMType::GRAPHICS, 0 },
    { "sza.17m",       0x400000, 0xe01b4588, ROMType::GRAPHICS, 0 },
    { "sza.19m",       0x400000, 0x0feeda64, ROMType::GRAPHICS, 0 },
    { "sza.14m",       0x100000, 0x0560c6aa, ROMType::GRAPHICS, 0 },
    { "sza.16m",       0x100000, 0xae940f87, ROMType::GRAPHICS, 0 },
    { "sza.18m",       0x100000, 0x4bc3c8bc, ROMType::GRAPHICS, 0 },
    { "sza.20m",       0x100000, 0x39e674c0, ROMType::GRAPHICS, 0 },
    { "sza.01",        0x020000, 0x1bc323cf, ROMType::SOUND_PROGRAM, 0 },
    { "sza.02",        0x020000, 0xba6a5013, ROMType::SOUND_PROGRAM, 0 },
    { "sza.11m",       0x200000, 0xaa47a601, ROMType::SOUND_SAMPLE, 0 },
    { "sza.12m",       0x200000, 0x2237bc53, ROMType::SOUND_SAMPLE, 0 },
    { "sfz2al.key",    0x000014, 0x2904963e, ROMType::ENCRYPTION_KEY, 0 },
};

// Super Gem Fighter: Mini Mix
static const ROMEntry sgemf_roms[] = {
    { "pcfu.03",       0x080000, 0xac2e8566, ROMType::PROGRAM, 0 },
    { "pcf.04",        0x080000, 0xf4314c96, ROMType::PROGRAM, 0 },
    { "pcf.05",        0x080000, 0x215655f6, ROMType::PROGRAM, 0 },
    { "pcf.06",        0x080000, 0xea6f13ea, ROMType::PROGRAM, 0 },
    { "pcf.07",        0x080000, 0x5ac6d5ea, ROMType::PROGRAM, 0 },
    { "pcf.13m",       0x400000, 0x22d72ab9, ROMType::GRAPHICS, 0 },
    { "pcf.15m",       0x400000, 0x16a4813c, ROMType::GRAPHICS, 0 },
    { "pcf.17m",       0x400000, 0x1097e035, ROMType::GRAPHICS, 0 },
    { "pcf.19m",       0x400000, 0xd362d874, ROMType::GRAPHICS, 0 },
    { "pcf.14m",       0x100000, 0x0383897c, ROMType::GRAPHICS, 0 },
    { "pcf.16m",       0x100000, 0x76f91084, ROMType::GRAPHICS, 0 },
    { "pcf.18m",       0x100000, 0x756c3754, ROMType::GRAPHICS, 0 },
    { "pcf.20m",       0x100000, 0x9ec9277d, ROMType::GRAPHICS, 0 },
    { "pcf.01",        0x020000, 0x254e5f33, ROMType::SOUND_PROGRAM, 0 },
    { "pcf.02",        0x020000, 0x6902f4f9, ROMType::SOUND_PROGRAM, 0 },
    { "pcf.11m",       0x400000, 0xa5dea005, ROMType::SOUND_SAMPLE, 0 },
    { "pcf.12m",       0x400000, 0x4ce235fe, ROMType::SOUND_SAMPLE, 0 },
    { "sgemf.key",     0x000014, 0x3d604021, ROMType::ENCRYPTION_KEY, 0 },
};

// Vampire Hunter 2: Darkstalkers Revenge
static const ROMEntry vhunt2_roms[] = {
    { "vh2j.03a",      0x080000, 0x9ae8f186, ROMType::PROGRAM, 0 },
    { "vh2j.04a",      0x080000, 0xe2fabf53, ROMType::PROGRAM, 0 },
    { "vh2j.05",       0x080000, 0xde34f624, ROMType::PROGRAM, 0 },
    { "vh2j.06",       0x080000, 0x6a3b9897, ROMType::PROGRAM, 0 },
    { "vh2j.07",       0x080000, 0xb021c029, ROMType::PROGRAM, 0 },
    { "vh2j.08",       0x080000, 0xac873dff, ROMType::PROGRAM, 0 },
    { "vh2j.09",       0x080000, 0xeaefce9c, ROMType::PROGRAM, 0 },
    { "vh2j.10",       0x080000, 0x11730952, ROMType::PROGRAM, 0 },
    { "vh2.13m",       0x400000, 0x3b02ddaa, ROMType::GRAPHICS, 0 },
    { "vh2.15m",       0x400000, 0x4e40de66, ROMType::GRAPHICS, 0 },
    { "vh2.17m",       0x400000, 0xb31d00c9, ROMType::GRAPHICS, 0 },
    { "vh2.19m",       0x400000, 0x149be3ab, ROMType::GRAPHICS, 0 },
    { "vh2.14m",       0x400000, 0xcd09bd63, ROMType::GRAPHICS, 0 },
    { "vh2.16m",       0x400000, 0xe0182c15, ROMType::GRAPHICS, 0 },
    { "vh2.18m",       0x400000, 0x778dc4f6, ROMType::GRAPHICS, 0 },
    { "vh2.20m",       0x400000, 0x605d9d1d, ROMType::GRAPHICS, 0 },
    { "vh2.01",        0x020000, 0x67b9f779, ROMType::SOUND_PROGRAM, 0 },
    { "vh2.02",        0x020000, 0xaaf15fcb, ROMType::SOUND_PROGRAM, 0 },
    { "vh2.11m",       0x400000, 0x38922efd, ROMType::SOUND_SAMPLE, 0 },
    { "vh2.12m",       0x400000, 0x6e2430af, ROMType::SOUND_SAMPLE, 0 },
    { "vhunt2.key",    0x000014, 0x61306b20, ROMType::ENCRYPTION_KEY, 0 },
};

// ============================================================================
// Game Database
// ============================================================================

const GameInfo GameDatabase::s_cps2_games[] = {
    {
        "19xx", "19XX: The War Against Destiny", 2, game_19xx_roms, sizeof(game_19xx_roms) / sizeof(ROMEntry),
        GameFlags::GAME_FLAG_VERTICAL_SCREEN, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr
    },
    {
        "1944", "1944: The Loop Master", 2, game_1944_roms, sizeof(game_1944_roms) / sizeof(ROMEntry),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr
    },
    {
        "armwar", "Armored Warriors", 2, armwar_roms, sizeof(armwar_roms) / sizeof(ROMEntry),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr
    },
    {
        "avsp", "Alien vs. Predator", 2, avsp_roms, sizeof(avsp_roms) / sizeof(ROMEntry),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr
    },
    {
        "batcir", "Battle Circuit", 2, batcir_roms, sizeof(batcir_roms) / sizeof(ROMEntry),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr
    },
    {
        "choko", "Janpai Puzzle Choukou", 2, choko_roms, sizeof(choko_roms) / sizeof(ROMEntry),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr
    },
    {
        "csclub", "Capcom Sports Club", 2, csclub_roms, sizeof(csclub_roms) / sizeof(ROMEntry),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr
    },
    {
        "cybots", "Cyberbots: Fullmetal Madness", 2, cybots_roms, sizeof(cybots_roms) / sizeof(ROMEntry),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr 
    },
    {
        "ddsom", "Dungeons & Dragons: Shadow over Mystara", 2, ddsom_roms, sizeof(ddsom_roms) / sizeof(ROMEntry),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr 
    },
    {
        "ddtod", "Dungeons & Dragons: Tower of Doom", 2, ddtod_roms, sizeof(ddtod_roms) / sizeof(ROMEntry),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr 
    },
    {
        "dimahoo", "Dimahoo", 2, dimahoo_roms, sizeof(dimahoo_roms) / sizeof(ROMEntry),
        GameFlags::GAME_FLAG_VERTICAL_SCREEN, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr 
    },
    {
        "dstlk", "Darkstalkers: The Night Warriors", 2, dstlk_roms, sizeof(dstlk_roms) / sizeof(ROMEntry),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr 
    },
    {
        "ecofghtr", "Eco Fighters", 2, ecofghtr_roms, sizeof(ecofghtr_roms) / sizeof(ROMEntry),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr 
    },
    {
        "ffightaec2", "Final Fight: Anniversary Edition", 2, ffightaec2_roms, sizeof(ffightaec2_roms) / sizeof(ROMEntry),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr 
    },
    {
        "gigawing", "Giga Wing", 2, gigawing_roms, sizeof(gigawing_roms) / sizeof(ROMEntry),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr 
    },
    {
        "hsf2", "Hyper Street Fighter II: The Anniversary Edition", 2, hsf2_roms, sizeof(hsf2_roms) / sizeof(ROMEntry),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr 
    },
    {
        "jyangoku", "Jyangokushi: Haoh no Saihai", 2, jyangoku_roms, sizeof(jyangoku_roms) / sizeof(ROMEntry),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr 
    },
    {
        "megaman2", "Mega Man 2: The Power Fighters", 2, megaman2_roms, sizeof(megaman2_roms) / sizeof(ROMEntry),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr 
    },
    {
        "mmatrix", "Mars Matrix: Hyper Solid Shooting", 2, mmatrix_roms, sizeof(mmatrix_roms) / sizeof(ROMEntry),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr 
    },
    {
        "mpang", "Mighty! Pang", 2, mpang_roms, sizeof(mpang_roms) / sizeof(ROMEntry),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr 
    },
    {
        "msh", "Marvel Super Heroes", 2, msh_roms, sizeof(msh_roms) / sizeof(ROMEntry),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr 
    },
    {
        "mshvsf", "Marvel Super Heroes vs. Street Fighter", 2, mshvsf_roms, sizeof(mshvsf_roms) / sizeof(ROMEntry),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr 
    },
    {
        "mvsc", "Marvel vs. Capcom: Clash of Super Heroes", 2, mvsc_roms, sizeof(mvsc_roms) / sizeof(ROMEntry),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr 
    },
    {
        "nwarr", "Night Warriors: Darkstalkers' Revenge", 2, nwarr_roms, sizeof(nwarr_roms) / sizeof(ROMEntry),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr 
    },
    {
        "progear", "Progear", 2, progear_roms, sizeof(progear_roms) / sizeof(ROMEntry),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr 
    },
    {
        "pzloop2", "Puzz Loop 2", 2, pzloop2_roms, sizeof(pzloop2_roms) / sizeof(ROMEntry),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr 
    },
    {
        "qndream", "Quiz Nanairo Dreams: Nijiirochou no Kiseki", 2, qndream_roms, sizeof(qndream_roms) / sizeof(ROMEntry),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr 
    },
    {
        "ringdest", "Ring of Destruction: Slammasters II", 2, ringdest_roms, sizeof(ringdest_roms) / sizeof(ROMEntry),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr 
    },
    {
        "sfa", "Street Fighter Alpha: Warriors' Dreams", 2, sfa_roms, sizeof(sfa_roms) / sizeof(ROMEntry),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr 
    },
    {
        "sfa2", "Street Fighter Alpha 2", 2, sfa2_roms, sizeof(sfa2_roms) / sizeof(ROMEntry),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr 
    },
    {
        "sfz2al", "Street Fighter Zero 2 Alpha", 2, sfz2al_roms, sizeof(sfz2al_roms) / sizeof(ROMEntry),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr 
    },
    {
        "sfa3", "Street Fighter Alpha 3", 2, sfa3_roms, sizeof(sfa3_roms) / sizeof(ROMEntry),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr 
    },
    {
        "sgemf", "Super Gem Fighter: Mini Mix", 2, sgemf_roms, sizeof(sgemf_roms) / sizeof(ROMEntry),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr 
    },
    {
        "spf2t", "Super Puzzle Fighter II Turbo", 2, spf2t_roms, sizeof(spf2t_roms) / sizeof(ROMEntry),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr 
    },
    {
        "ssf2", "Super Street Fighter II: The New Challengers", 2, ssf2_roms, sizeof(ssf2_roms) / sizeof(ROMEntry),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr 
    },
    {
        "ssf2t", "Super Street Fighter II Turbo", 2, ssf2t_roms, sizeof(ssf2t_roms) / sizeof(ROMEntry),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr 
    },
    {
        "vhunt2", "Vampire Hunter 2: Darkstalkers Revenge", 2, vhunt2_roms, sizeof(vhunt2_roms) / sizeof(ROMEntry),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr 
    },
    {
        "vsav", "Vampire Savior: The Lord of Vampire", 2, vsav_roms, sizeof(vsav_roms) / sizeof(ROMEntry),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr 
    },
    {
        "vsav2", "Vampire Savior 2: The Lord of Vampire", 2, vsav2_roms, sizeof(vsav2_roms) / sizeof(ROMEntry),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr 
    },
    {
        "xmcota", "X-Men: Children of the Atom", 2, xmcota_roms, sizeof(xmcota_roms) / sizeof(ROMEntry),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr 
    },
    {
        "xmvsf", "X-Men vs. Street Fighter", 2, xmvsf_roms, sizeof(xmvsf_roms) / sizeof(ROMEntry),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_NONE, nullptr 
    },
};

const u32 GameDatabase::s_cps2_gameCount = static_cast<u32>(sizeof(s_cps2_games) / sizeof(s_cps2_games[0]));

} // namespace cps
