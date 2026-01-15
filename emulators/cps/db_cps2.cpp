#include "db.h"
#include "../../../utilities/miniz/miniz.h"
#include <algorithm>
#include <cstring>

namespace cps {

// ============================================================================
// ROM Definitions
// ============================================================================

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
static const ROMEntry game_avsp_roms[] = {
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
static const ROMEntry game_ddsom_roms[] = {
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
static const ROMEntry game_ddtod_roms[] = {
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
static const ROMEntry game_hsf2_roms[] = {
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
static const ROMEntry game_msh_roms[] = {
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
static const ROMEntry game_mshvsf_roms[] = {
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
static const ROMEntry game_mvsc_roms[] = {
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
static const ROMEntry game_progear_roms[] = {
    { "pgae.03",       0x080000, 0x8577bc86, ROMType::PROGRAM, 0 },
    { "pgae.04",       0x080000, 0xd850da04, ROMType::PROGRAM, 0 },
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
static const ROMEntry game_sfa_roms[] = {
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
static const ROMEntry game_sfa2_roms[] = {
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
static const ROMEntry game_sfa3_roms[] = {
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
static const ROMEntry game_spf2t_roms[] = {
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
static const ROMEntry game_ssf2_roms[] = {
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
static const ROMEntry game_ssf2t_roms[] = {
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

// Super Street Fighter II Turbo (Brazil)
static const ROMEntry game_ssf2tb_roms[] = {
    { "ssfe.03tc",     0x080000, 0x496a8409, ROMType::PROGRAM, 0 },
    { "ssfe.04tc",     0x080000, 0x4b45c18b, ROMType::PROGRAM, 0 },
    { "ssfe.05t",      0x080000, 0x6a9c6444, ROMType::PROGRAM, 0 },
    { "ssfe.06tb",     0x080000, 0xe4944fc3, ROMType::PROGRAM, 0 },
    { "ssfe.07t",      0x080000, 0x2c9f4782, ROMType::PROGRAM, 0 },
    { "ssf.13m",       0x200000, 0xcf94d275, ROMType::GRAPHICS, 0 },
    { "ssf.15m",       0x200000, 0x5eb703af, ROMType::GRAPHICS, 0 },
    { "ssf.17m",       0x200000, 0xffa60e0f, ROMType::GRAPHICS, 0 },
    { "ssf.19m",       0x200000, 0x34e825c5, ROMType::GRAPHICS, 0 },
    { "ssf.14m",       0x100000, 0xb7cc32e7, ROMType::GRAPHICS, 0 },
    { "ssf.16m",       0x100000, 0x8376ad18, ROMType::GRAPHICS, 0 },
    { "ssf.18m",       0x100000, 0xf5b1b336, ROMType::GRAPHICS, 0 },
    { "ssf.20m",       0x100000, 0x459d5c6b, ROMType::GRAPHICS, 0 },
    { "ssf.01",        0x020000, 0xeb247e8c, ROMType::SOUND_PROGRAM, 0 },
    { "ssf.q01",       0x080000, 0xa6f9da5c, ROMType::SOUND_SAMPLE, 0 },
    { "ssf.q02",       0x080000, 0x8c66ae26, ROMType::SOUND_SAMPLE, 0 },
    { "ssf.q03",       0x080000, 0x695cc2ca, ROMType::SOUND_SAMPLE, 0 },
    { "ssf.q04",       0x080000, 0x9d9ebe32, ROMType::SOUND_SAMPLE, 0 },
    { "ssf.q05",       0x080000, 0x4770e7b7, ROMType::SOUND_SAMPLE, 0 },
    { "ssf.q06",       0x080000, 0x4e79c951, ROMType::SOUND_SAMPLE, 0 },
    { "ssf.q07",       0x080000, 0xcdd14313, ROMType::SOUND_SAMPLE, 0 },
    { "ssf.q08",       0x080000, 0x6f5a088c, ROMType::SOUND_SAMPLE, 0 },
    { "ssf2tb.key",    0x000014, 0x1ecc92b2, ROMType::ENCRYPTION_KEY, 0 },
};

// Vampire Savior: The Lord of Vampire
static const ROMEntry game_vsav_roms[] = {
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
static const ROMEntry game_vsav2_roms[] = {
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
static const ROMEntry game_xmcota_roms[] = {
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
static const ROMEntry game_xmvsf_roms[] = {
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

// ============================================================================
// Game Database
// ============================================================================

const GameInfo GameDatabase::s_cps2_games[] = {
    {
        2,
        "1944: The Loop Master",
        "1944",
        game_1944_roms,
        sizeof(game_1944_roms) / sizeof(ROMEntry),
        CPSBoard::CPS_B_UNUSED,
        CPSMapper::MAPPER_UNUSED
    },
    {
        2,
        "Alien vs. Predator",
        "avsp",
        game_avsp_roms,
        sizeof(game_avsp_roms) / sizeof(ROMEntry),
        CPSBoard::CPS_B_UNUSED,
        CPSMapper::MAPPER_UNUSED
    },
    {
        2,
        "Dungeons & Dragons: Shadow over Mystara",
        "ddsom",
        game_ddsom_roms,
        sizeof(game_ddsom_roms) / sizeof(ROMEntry),
        CPSBoard::CPS_B_UNUSED,
        CPSMapper::MAPPER_UNUSED
    },
    {
        2,
        "Dungeons & Dragons: Tower of Doom",
        "ddtod",
        game_ddtod_roms,
        sizeof(game_ddtod_roms) / sizeof(ROMEntry),
        CPSBoard::CPS_B_UNUSED,
        CPSMapper::MAPPER_UNUSED
    },
    {
        2,
        "Hyper Street Fighter II: The Anniversary Edition",
        "hsf2",
        game_hsf2_roms,
        sizeof(game_hsf2_roms) / sizeof(ROMEntry),
        CPSBoard::CPS_B_UNUSED,
        CPSMapper::MAPPER_UNUSED
    },
    {
        2,
        "Marvel Super Heroes",
        "msh",
        game_msh_roms,
        sizeof(game_msh_roms) / sizeof(ROMEntry),
        CPSBoard::CPS_B_UNUSED,
        CPSMapper::MAPPER_UNUSED
    },
    {
        2,
        "Marvel Super Heroes vs. Street Fighter",
        "mshvsf",
        game_mshvsf_roms,
        sizeof(game_mshvsf_roms) / sizeof(ROMEntry),
        CPSBoard::CPS_B_UNUSED,
        CPSMapper::MAPPER_UNUSED
    },
    {
        2,
        "Marvel vs. Capcom: Clash of Super Heroes",
        "mvsc",
        game_mvsc_roms,
        sizeof(game_mvsc_roms) / sizeof(ROMEntry),
        CPSBoard::CPS_B_UNUSED,
        CPSMapper::MAPPER_UNUSED
    },
    {
        2,
        "Progear",
        "progear",
        game_progear_roms,
        sizeof(game_progear_roms) / sizeof(ROMEntry),
        CPSBoard::CPS_B_UNUSED,
        CPSMapper::MAPPER_UNUSED
    },
    {
        2,
        "Street Fighter Alpha: Warriors' Dreams",
        "sfa",
        game_sfa_roms,
        sizeof(game_sfa_roms) / sizeof(ROMEntry),
        CPSBoard::CPS_B_UNUSED,
        CPSMapper::MAPPER_UNUSED
    },
    {
        2,
        "Street Fighter Alpha 2",
        "sfa2",
        game_sfa2_roms,
        sizeof(game_sfa2_roms) / sizeof(ROMEntry),
        CPSBoard::CPS_B_UNUSED,
        CPSMapper::MAPPER_UNUSED
    },
    {
        2,
        "Street Fighter Alpha 3",
        "sfa3",
        game_sfa3_roms,
        sizeof(game_sfa3_roms) / sizeof(ROMEntry),
        CPSBoard::CPS_B_UNUSED,
        CPSMapper::MAPPER_UNUSED
    },
    {
        2,
        "Super Puzzle Fighter II Turbo",
        "spf2t",
        game_spf2t_roms,
        sizeof(game_spf2t_roms) / sizeof(ROMEntry),
        CPSBoard::CPS_B_UNUSED,
        CPSMapper::MAPPER_UNUSED
    },
    {
        2,
        "Super Street Fighter II: The New Challengers",
        "ssf2",
        game_ssf2_roms,
        sizeof(game_ssf2_roms) / sizeof(ROMEntry),
        CPSBoard::CPS_B_UNUSED,
        CPSMapper::MAPPER_UNUSED
    },
    {
        2,
        "Super Street Fighter II Turbo",
        "ssf2t",
        game_ssf2t_roms,
        sizeof(game_ssf2t_roms) / sizeof(ROMEntry),
        CPSBoard::CPS_B_UNUSED,
        CPSMapper::MAPPER_UNUSED
    },
    {
        2,
        "Super Street Fighter II Turbo (Brazil)",
        "ssf2tb",
        game_ssf2tb_roms,
        sizeof(game_ssf2tb_roms) / sizeof(ROMEntry),
        CPSBoard::CPS_B_UNUSED,
        CPSMapper::MAPPER_UNUSED
    },
    {
        2,
        "Vampire Savior: The Lord of Vampire",
        "vsav",
        game_vsav_roms,
        sizeof(game_vsav_roms) / sizeof(ROMEntry),
        CPSBoard::CPS_B_UNUSED,
        CPSMapper::MAPPER_UNUSED
    },
    {
        2,
        "Vampire Savior 2: The Lord of Vampire",
        "vsav2",
        game_vsav2_roms,
        sizeof(game_vsav2_roms) / sizeof(ROMEntry),
        CPSBoard::CPS_B_UNUSED,
        CPSMapper::MAPPER_UNUSED
    },
    {
        2,
        "X-Men: Children of the Atom",
        "xmcota",
        game_xmcota_roms,
        sizeof(game_xmcota_roms) / sizeof(ROMEntry),
        CPSBoard::CPS_B_UNUSED,
        CPSMapper::MAPPER_UNUSED
    },
    {
        2,
        "X-Men vs. Street Fighter",
        "xmvsf",
        game_xmvsf_roms,
        sizeof(game_xmvsf_roms) / sizeof(ROMEntry),
        CPSBoard::CPS_B_UNUSED,
        CPSMapper::MAPPER_UNUSED
    },
};

const u32 GameDatabase::s_cps2_gameCount = static_cast<u32>(sizeof(s_cps2_games) / sizeof(s_cps2_games[0]));

} // namespace cps
