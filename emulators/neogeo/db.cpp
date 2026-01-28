#include "db.h"
#include "../../../utilities/miniz/miniz.h"
#include <algorithm>
#include <cstring>

namespace neogeo {

// ============================================================================
// Game Database
// ============================================================================

// 19YY (Neo CD conversion, ADK World)
static const ROMEntry _19yy_roms[] = {
    { "19yy-p1.p1",    0x00200000, 0x59374c47, ROMType::PROGRAM, 0 },
    { "19yy-s1.s1",    0x00020000, 0x219b6f40, ROMType::TEXT, 0 },
    { "19yy-c1.c1",    0x00400000, 0x622719d5, ROMType::SPRITE, 0 },
    { "19yy-c2.c2",    0x00400000, 0x41b07be5, ROMType::SPRITE, 0 },
    { "19yy-m1.m1",    0x00020000, 0x8e05762a, ROMType::SOUND_PROGRAM, 0 },
    { "19yy-v1.v1",    0x00800000, 0x944146c2, ROMType::SOUND_SAMPLE, 0 },
    { "19yy-v2.v2",    0x00800000, 0xa4bafe45, ROMType::SOUND_SAMPLE, 0 },
};

// 2020 Super Baseball (set 1)
static const ROMEntry _2020bb_roms[] = {
    { "030-p1.p1",    0x00080000, 0xd396c9cb, ROMType::PROGRAM, 0 },
    { "030-s1.s1",    0x00020000, 0x7015b8fc, ROMType::TEXT, 0 },
    { "030-c1.c1",    0x00100000, 0x4f5e19bd, ROMType::SPRITE, 0 },
    { "030-c2.c2",    0x00100000, 0xd6314bf0, ROMType::SPRITE, 0 },
    { "030-c3.c3",    0x00100000, 0x47fddfee, ROMType::SPRITE, 0 },
    { "030-c4.c4",    0x00100000, 0x780d1c4e, ROMType::SPRITE, 0 },
    { "030-m1.m1",    0x00020000, 0x4cf466ec, ROMType::SOUND_PROGRAM, 0 },
    { "030-v1.v1",    0x00100000, 0xd4ca364e, ROMType::SOUND_SAMPLE, 0 },
    { "030-v2.v2",    0x00100000, 0x54994455, ROMType::SOUND_SAMPLE, 0 },
};

// 3 Count Bout / Fire Suplex (NGM-043 ~ NGH-043)
static const ROMEntry _3countb_roms[] = {
    { "043-p1.p1",    0x00100000, 0xffbdd928, ROMType::PROGRAM, 0 },
    { "043-s1.s1",    0x00020000, 0xc362d484, ROMType::TEXT, 0 },
    { "043-c1.c1",    0x00200000, 0xbad2d67f, ROMType::SPRITE, 0 },
    { "043-c2.c2",    0x00200000, 0xa7fbda95, ROMType::SPRITE, 0 },
    { "043-c3.c3",    0x00200000, 0xf00be011, ROMType::SPRITE, 0 },
    { "043-c4.c4",    0x00200000, 0x1887e5c0, ROMType::SPRITE, 0 },
    { "043-m1.m1",    0x00020000, 0x7eab59cb, ROMType::SOUND_PROGRAM, 0 },
    { "043-v1.v1",    0x00200000, 0x63688ce8, ROMType::SOUND_SAMPLE, 0 },
    { "043-v2.v2",    0x00200000, 0xc69a827b, ROMType::SOUND_SAMPLE, 0 },
};

// Aero Fighters 2 / Sonic Wings 2
static const ROMEntry sonicwi2_roms[] = {
    { "075-p1.p1",    0x00200000, 0x92871738, ROMType::PROGRAM, 0 },
    { "075-s1.s1",    0x00020000, 0xc9eec367, ROMType::TEXT, 0 },
    { "075-c1.c1",    0x00200000, 0x3278e73e, ROMType::SPRITE, 0 },
    { "075-c2.c2",    0x00200000, 0xfe6355d6, ROMType::SPRITE, 0 },
    { "075-c3.c3",    0x00200000, 0xc1b438f1, ROMType::SPRITE, 0 },
    { "075-c4.c4",    0x00200000, 0x1f777206, ROMType::SPRITE, 0 },
    { "075-m1.m1",    0x00020000, 0xbb828df1, ROMType::SOUND_PROGRAM, 0 },
    { "075-v1.v1",    0x00200000, 0x7577e949, ROMType::SOUND_SAMPLE, 0 },
    { "075-v2.v2",    0x00100000, 0x021760cd, ROMType::SOUND_SAMPLE, 0 },
};

// Aero Fighters 3 / Sonic Wings 3
static const ROMEntry sonicwi3_roms[] = {
    { "097-p1.p1",    0x00200000, 0x0547121d, ROMType::PROGRAM, 0 },
    { "097-s1.s1",    0x00020000, 0x8dd66743, ROMType::TEXT, 0 },
    { "097-c1.c1",    0x00400000, 0x33d0d589, ROMType::SPRITE, 0 },
    { "097-c2.c2",    0x00400000, 0x186f8b43, ROMType::SPRITE, 0 },
    { "097-c3.c3",    0x00200000, 0xc339fff5, ROMType::SPRITE, 0 },
    { "097-c4.c4",    0x00200000, 0x84a40c6e, ROMType::SPRITE, 0 },
    { "097-m1.m1",    0x00020000, 0xb20e4291, ROMType::SOUND_PROGRAM, 0 },
    { "097-v1.v1",    0x00400000, 0x6f885152, ROMType::SOUND_SAMPLE, 0 },
    { "097-v2.v2",    0x00200000, 0x3359e868, ROMType::SOUND_SAMPLE, 0 },
};

// Aggressors of Dark Kombat / Tsuukai GANGAN Koushinkyoku (ADM-008 ~ ADH-008)
static const ROMEntry aodk_roms[] = {
    { "074-p1.p1",    0x00200000, 0x62369553, ROMType::PROGRAM, 0 },
    { "074-s1.s1",    0x00020000, 0x96148d2b, ROMType::TEXT, 0 },
    { "074-c1.c1",    0x00200000, 0xa0b39344, ROMType::SPRITE, 0 },
    { "074-c2.c2",    0x00200000, 0x203f6074, ROMType::SPRITE, 0 },
    { "074-c3.c3",    0x00200000, 0x7fff4d41, ROMType::SPRITE, 0 },
    { "074-c4.c4",    0x00200000, 0x48db3e0a, ROMType::SPRITE, 0 },
    { "074-c5.c5",    0x00200000, 0xc74c5e51, ROMType::SPRITE, 0 },
    { "074-c6.c6",    0x00200000, 0x73e8e7e0, ROMType::SPRITE, 0 },
    { "074-c7.c7",    0x00200000, 0xac7daa01, ROMType::SPRITE, 0 },
    { "074-c8.c8",    0x00200000, 0x14e7ad71, ROMType::SPRITE, 0 },
    { "074-m1.m1",    0x00020000, 0x5a52a9d1, ROMType::SOUND_PROGRAM, 0 },
    { "074-v1.v1",    0x00200000, 0x7675b8fa, ROMType::SOUND_SAMPLE, 0 },
    { "074-v2.v2",    0x00200000, 0xa9da86e9, ROMType::SOUND_SAMPLE, 0 },
};

// Alpha Mission II / ASO II - Last Guardian (NGM-007 ~ NGH-007)
static const ROMEntry alpham2_roms[] = {
    { "007-p1.p1",    0x00080000, 0x5b266f47, ROMType::PROGRAM, 0 },
    { "007-p2.p2",    0x00020000, 0xeb9c1044, ROMType::PROGRAM, 0 },
    { "007-s1.s1",    0x00020000, 0x85ec9acf, ROMType::TEXT, 0 },
    { "007-c1.c1",    0x00100000, 0x8fba8ff3, ROMType::SPRITE, 0 },
    { "007-c2.c2",    0x00100000, 0x4dad2945, ROMType::SPRITE, 0 },
    { "007-c3.c3",    0x00080000, 0x68c2994e, ROMType::SPRITE, 0 },
    { "007-c4.c4",    0x00080000, 0x7d588349, ROMType::SPRITE, 0 },
    { "007-m1.m1",    0x00020000, 0x28dfe2cd, ROMType::SOUND_PROGRAM, 0 },
    { "007-v1.v1",    0x00100000, 0xcd5db931, ROMType::SOUND_SAMPLE, 0 },
    { "007-v2.v2",    0x00100000, 0x63e9b574, ROMType::SOUND_SAMPLE, 0 },
};

// Andro Dunos (NGM-049 ~ NGH-049)
static const ROMEntry androdun_roms[] = {
    { "049-p1.p1",    0x00080000, 0x3b857da2, ROMType::PROGRAM, 0 },
    { "049-p2.p2",    0x00080000, 0x2f062209, ROMType::PROGRAM, 0 },
    { "049-s1.s1",    0x00020000, 0x6349de5d, ROMType::TEXT, 0 },
    { "049-c1.c1",    0x00100000, 0x7ace6db3, ROMType::SPRITE, 0 },
    { "049-c2.c2",    0x00100000, 0xb17024f7, ROMType::SPRITE, 0 },
    { "049-m1.m1",    0x00020000, 0xedd2acf4, ROMType::SOUND_PROGRAM, 0 },
    { "049-v1.v1",    0x00100000, 0xce43cb89, ROMType::SOUND_SAMPLE, 0 },
};

// Art of Fighting / Ryuuko no Ken (NGM-044 ~ NGH-044)
static const ROMEntry aof_roms[] = {
    { "044-p1.p1",    0x00080000, 0xca9f7a6d, ROMType::PROGRAM, 0 },
    { "044-s1.s1",    0x00020000, 0x89903f39, ROMType::TEXT, 0 },
    { "044-c1.c1",    0x00200000, 0xddab98a7, ROMType::SPRITE, 0 },
    { "044-c2.c2",    0x00200000, 0xd8ccd575, ROMType::SPRITE, 0 },
    { "044-c3.c3",    0x00200000, 0x403e898a, ROMType::SPRITE, 0 },
    { "044-c4.c4",    0x00200000, 0x6235fbaa, ROMType::SPRITE, 0 },
    { "044-m1.m1",    0x00020000, 0x0987e4bb, ROMType::SOUND_PROGRAM, 0 },
    { "044-v2.v2",    0x00200000, 0x3ec632ea, ROMType::SOUND_SAMPLE, 0 },
    { "044-v4.v4",    0x00200000, 0x4b0f8e23, ROMType::SOUND_SAMPLE, 0 },
};

// Art of Fighting 2 / Ryuuko no Ken 2 (NGM-056)
static const ROMEntry aof2_roms[] = {
    { "056-p1.p1",    0x00100000, 0xa3b1d021, ROMType::PROGRAM, 0 },
    { "056-s1.s1",    0x00020000, 0x8b02638e, ROMType::TEXT, 0 },
    { "056-c1.c1",    0x00200000, 0x17b9cbd2, ROMType::SPRITE, 0 },
    { "056-c2.c2",    0x00200000, 0x5fd76b67, ROMType::SPRITE, 0 },
    { "056-c3.c3",    0x00200000, 0xd2c88768, ROMType::SPRITE, 0 },
    { "056-c4.c4",    0x00200000, 0xdb39b883, ROMType::SPRITE, 0 },
    { "056-c5.c5",    0x00200000, 0xc3074137, ROMType::SPRITE, 0 },
    { "056-c6.c6",    0x00200000, 0x31de68d3, ROMType::SPRITE, 0 },
    { "056-c7.c7",    0x00200000, 0x3f36df57, ROMType::SPRITE, 0 },
    { "056-c8.c8",    0x00200000, 0xe546d7a8, ROMType::SPRITE, 0 },
    { "056-m1.m1",    0x00020000, 0xf27e9d52, ROMType::SOUND_PROGRAM, 0 },
    { "056-v1.v1",    0x00200000, 0x4628fde0, ROMType::SOUND_SAMPLE, 0 },
    { "056-v2.v2",    0x00200000, 0xb710e2f2, ROMType::SOUND_SAMPLE, 0 },
    { "056-v3.v3",    0x00100000, 0xd168c301, ROMType::SOUND_SAMPLE, 0 },
};

// Art of Fighting 3 - The Path of the Warrior / Art of Fighting - Ryuuko no Ken Gaiden
static const ROMEntry aof3_roms[] = {
    { "096-p1.p1",    0x00100000, 0x9edb420d, ROMType::PROGRAM, 0 },
    { "096-p2.sp2",   0x00200000, 0x4d5a2602, ROMType::PROGRAM, 0 },
    { "096-s1.s1",    0x00020000, 0xcc7fd344, ROMType::TEXT, 0 },
    { "096-c1.c1",    0x00400000, 0xf17b8d89, ROMType::SPRITE, 0 },
    { "096-c2.c2",    0x00400000, 0x3840c508, ROMType::SPRITE, 0 },
    { "096-c3.c3",    0x00400000, 0x55f9ee1e, ROMType::SPRITE, 0 },
    { "096-c4.c4",    0x00400000, 0x585b7e47, ROMType::SPRITE, 0 },
    { "096-c5.c5",    0x00400000, 0xc75a753c, ROMType::SPRITE, 0 },
    { "096-c6.c6",    0x00400000, 0x9a9d2f7a, ROMType::SPRITE, 0 },
    { "096-c7.c7",    0x00200000, 0x51bd8ab2, ROMType::SPRITE, 0 },
    { "096-c8.c8",    0x00200000, 0x9a34f99c, ROMType::SPRITE, 0 },
    { "096-m1.m1",    0x00020000, 0xcb07b659, ROMType::SOUND_PROGRAM, 0 },
    { "096-v1.v1",    0x00200000, 0xe2c32074, ROMType::SOUND_SAMPLE, 0 },
    { "096-v2.v2",    0x00200000, 0xa290eee7, ROMType::SOUND_SAMPLE, 0 },
    { "096-v3.v3",    0x00200000, 0x199d12ea, ROMType::SOUND_SAMPLE, 0 },
};

// Bakatonosama Mahjong Manyuuki (MOM-002 ~ MOH-002)
static const ROMEntry bakatono_roms[] = {
    { "036-p1.p1",    0x00080000, 0x1c66b6fa, ROMType::PROGRAM, 0 },
    { "036-s1.s1",    0x00020000, 0xf3ef4485, ROMType::TEXT, 0 },
    { "036-c1.c1",    0x00100000, 0xfe7f1010, ROMType::SPRITE, 0 },
    { "036-c2.c2",    0x00100000, 0xbbf003f5, ROMType::SPRITE, 0 },
    { "036-c3.c3",    0x00100000, 0x9ac0708e, ROMType::SPRITE, 0 },
    { "036-c4.c4",    0x00100000, 0xf2577d22, ROMType::SPRITE, 0 },
    { "036-m1.m1",    0x00020000, 0xf1385b96, ROMType::SOUND_PROGRAM, 0 },
    { "036-v1.v1",    0x00100000, 0x1c335dce, ROMType::SOUND_SAMPLE, 0 },
    { "036-v2.v2",    0x00100000, 0xbbf79342, ROMType::SOUND_SAMPLE, 0 },
};

// Bang Bang Busters (2010 NCI release)
static const ROMEntry b2b_roms[] = {
    { "071.p1",    0x00080000, 0x7687197d, ROMType::PROGRAM, 0 },
    { "071.s1",    0x00020000, 0x44e5f154, ROMType::TEXT, 0 },
    { "071.c1",    0x00200000, 0x23d84a7a, ROMType::SPRITE, 0 },
    { "071.c2",    0x00200000, 0xce7b6248, ROMType::SPRITE, 0 },
    { "071.m1",    0x00020000, 0x6da739ad, ROMType::SOUND_PROGRAM, 0 },
    { "071.v1",    0x00100000, 0x50feffb0, ROMType::SOUND_SAMPLE, 0 },
};

// Bang Bead
static const ROMEntry bangbead_roms[] = {
    { "259-p1.p1",    0x00200000, 0x88a37f8b, ROMType::PROGRAM, 0 },
    { "259-c1.c1",    0x00800000, 0x1f537f74, ROMType::SPRITE, 0 },
    { "259-c2.c2",    0x00800000, 0x0efd98ff, ROMType::SPRITE, 0 },
    { "259-m1.m1",    0x00020000, 0x85668ee9, ROMType::SOUND_PROGRAM, 0 },
    { "259-v1.v1",    0x00400000, 0x088eb8ab, ROMType::SOUND_SAMPLE, 0 },
    { "259-v2.v2",    0x00100000, 0x97528fe9, ROMType::SOUND_SAMPLE, 0 },
};

// Baseball Stars 2
static const ROMEntry bstars2_roms[] = {
    { "041-p1.p1",    0x00080000, 0x523567fd, ROMType::PROGRAM, 0 },
    { "041-s1.s1",    0x00020000, 0x015c5c94, ROMType::TEXT, 0 },
    { "041-c1.c1",    0x00100000, 0xb39a12e1, ROMType::SPRITE, 0 },
    { "041-c2.c2",    0x00100000, 0x766cfc2f, ROMType::SPRITE, 0 },
    { "041-c3.c3",    0x00100000, 0xfb31339d, ROMType::SPRITE, 0 },
    { "041-c4.c4",    0x00100000, 0x70457a0c, ROMType::SPRITE, 0 },
    { "041-m1.m1",    0x00020000, 0x15c177a6, ROMType::SOUND_PROGRAM, 0 },
    { "041-v1.v1",    0x00100000, 0xcb1da093, ROMType::SOUND_SAMPLE, 0 },
    { "041-v2.v2",    0x00100000, 0x1c954a9d, ROMType::SOUND_SAMPLE, 0 },
    { "041-v3.v3",    0x00080000, 0xafaa0180, ROMType::SOUND_SAMPLE, 0 },
};

// Baseball Stars Professional (NGM-002)
static const ROMEntry bstars_roms[] = {
    { "002-pg.p1",    0x00080000, 0xc100b5f5, ROMType::PROGRAM, 0 },
    { "002-s1.s1",    0x00020000, 0x1a7fd0c6, ROMType::TEXT, 0 },
    { "002-c1.c1",    0x00080000, 0xaaff2a45, ROMType::SPRITE, 0 },
    { "002-c2.c2",    0x00080000, 0x3ba0f7e4, ROMType::SPRITE, 0 },
    { "002-c3.c3",    0x00080000, 0x96f0fdfa, ROMType::SPRITE, 0 },
    { "002-c4.c4",    0x00080000, 0x5fd87f2f, ROMType::SPRITE, 0 },
    { "002-c5.c5",    0x00080000, 0x807ed83b, ROMType::SPRITE, 0 },
    { "002-c6.c6",    0x00080000, 0x5a3cad41, ROMType::SPRITE, 0 },
    { "002-m1.m1",    0x00040000, 0x4ecaa4ee, ROMType::SOUND_PROGRAM, 0 },
    { "002-v11.v11",  0x00080000, 0xb7b925bd, ROMType::SOUND_SAMPLE, 0 },
    { "002-v12.v12",  0x00080000, 0x329f26fc, ROMType::SOUND_SAMPLE, 0 },
    { "002-v13.v13",  0x00080000, 0x0c39f3c8, ROMType::SOUND_SAMPLE, 0 },
    { "002-v14.v14",  0x00080000, 0xc7e11c38, ROMType::SOUND_SAMPLE, 0 },
    { "002-v21.v21",  0x00080000, 0x04a733d1, ROMType::SOUND_SAMPLE, 0 },
};

// Battle Flip Shot
static const ROMEntry flipshot_roms[] = {
    { "247-p1.p1",    0x00100000, 0x95779094, ROMType::PROGRAM, 0 },
    { "247-s1.s1",    0x00020000, 0x6300185c, ROMType::TEXT, 0 },
    { "247-c1.c1",    0x00200000, 0xc9eedcb2, ROMType::SPRITE, 0 },
    { "247-c2.c2",    0x00200000, 0x7d6d6e87, ROMType::SPRITE, 0 },
    { "247-m1.m1",    0x00020000, 0xa9fe0144, ROMType::SOUND_PROGRAM, 0 },
    { "247-v1.v1",    0x00200000, 0x42ec743d, ROMType::SOUND_SAMPLE, 0 },
};

// Blazing Star
static const ROMEntry blazstar_roms[] = {
    { "239-p1.p1",    0x00100000, 0x183682f8, ROMType::PROGRAM, 0 },
    { "239-p2.sp2",   0x00200000, 0x9a9f4154, ROMType::PROGRAM, 0 },
    { "239-s1.s1",    0x00020000, 0xd56cb498, ROMType::TEXT, 0 },
    { "239-c1.c1",    0x00400000, 0x84f6d584, ROMType::SPRITE, 0 },
    { "239-c2.c2",    0x00400000, 0x05a0cb22, ROMType::SPRITE, 0 },
    { "239-c3.c3",    0x00400000, 0x5fb69c9e, ROMType::SPRITE, 0 },
    { "239-c4.c4",    0x00400000, 0x0be028c4, ROMType::SPRITE, 0 },
    { "239-c5.c5",    0x00400000, 0x74bae5f8, ROMType::SPRITE, 0 },
    { "239-c6.c6",    0x00400000, 0x4e0700d2, ROMType::SPRITE, 0 },
    { "239-c7.c7",    0x00400000, 0x010ff4fd, ROMType::SPRITE, 0 },
    { "239-c8.c8",    0x00400000, 0xdb60460e, ROMType::SPRITE, 0 },
    { "239-m1.m1",    0x00020000, 0xd31a3aea, ROMType::SOUND_PROGRAM, 0 },
    { "239-v1.v1",    0x00400000, 0x1b8d5bf7, ROMType::SOUND_SAMPLE, 0 },
    { "239-v2.v2",    0x00400000, 0x74cf0a70, ROMType::SOUND_SAMPLE, 0 },
};

// Blue's Journey / Raguy (ALM-001 ~ ALH-001)
static const ROMEntry bjourney_roms[] = {
    { "022-p1.p1",    0x00100000, 0x6a2f6d4a, ROMType::PROGRAM, 0 },
    { "022-s1.s1",    0x00020000, 0x843c3624, ROMType::TEXT, 0 },
    { "022-c1.c1",    0x00100000, 0x4d47a48c, ROMType::SPRITE, 0 },
    { "022-c2.c2",    0x00100000, 0xe8c1491a, ROMType::SPRITE, 0 },
    { "022-c3.c3",    0x00080000, 0x66e69753, ROMType::SPRITE, 0 },
    { "022-c4.c4",    0x00080000, 0x71bfd48a, ROMType::SPRITE, 0 },
    { "022-m1.m1",    0x00020000, 0x8e1d4ab6, ROMType::SOUND_PROGRAM, 0 },
    { "022-v11.v11",  0x00100000, 0x2cb4ad91, ROMType::SOUND_SAMPLE, 0 },
    { "022-v22.v22",  0x00100000, 0x65a54d13, ROMType::SOUND_SAMPLE, 0 },
};

// Breakers Revenge
static const ROMEntry breakrev_roms[] = {
    { "245-p1.p1",    0x00200000, 0xc828876d, ROMType::PROGRAM, 0 },
    { "245-s1.s1",    0x00020000, 0xe7660a5d, ROMType::TEXT, 0 },
    { "245-c1.c1",    0x00400000, 0x68d4ae76, ROMType::SPRITE, 0 },
    { "245-c2.c2",    0x00400000, 0xfdee05cd, ROMType::SPRITE, 0 },
    { "245-c3.c3",    0x00400000, 0x645077f3, ROMType::SPRITE, 0 },
    { "245-c4.c4",    0x00400000, 0x63aeb74c, ROMType::SPRITE, 0 },
    { "245-c5.c5",    0x00400000, 0xb5f40e7f, ROMType::SPRITE, 0 },
    { "245-c6.c6",    0x00400000, 0xd0337328, ROMType::SPRITE, 0 },
    { "245-m1.m1",    0x00020000, 0x00f31c66, ROMType::SOUND_PROGRAM, 0 },
    { "245-v1.v1",    0x00400000, 0xe255446c, ROMType::SOUND_SAMPLE, 0 },
    { "245-v2.v2",    0x00400000, 0x9068198a, ROMType::SOUND_SAMPLE, 0 },
};

// Breakers
static const ROMEntry breakers_roms[] = {
    { "230-p1.p1",    0x00200000, 0xed24a6e6, ROMType::PROGRAM, 0 },
    { "230-s1.s1",    0x00020000, 0x076fb64c, ROMType::TEXT, 0 },
    { "230-c1.c1",    0x00400000, 0x68d4ae76, ROMType::SPRITE, 0 },
    { "230-c2.c2",    0x00400000, 0xfdee05cd, ROMType::SPRITE, 0 },
    { "230-c3.c3",    0x00400000, 0x645077f3, ROMType::SPRITE, 0 },
    { "230-c4.c4",    0x00400000, 0x63aeb74c, ROMType::SPRITE, 0 },
    { "230-m1.m1",    0x00020000, 0x3951a1c1, ROMType::SOUND_PROGRAM, 0 },
    { "230-v1.v1",    0x00400000, 0x7f9ed279, ROMType::SOUND_SAMPLE, 0 },
    { "230-v2.v2",    0x00400000, 0x1d43e420, ROMType::SOUND_SAMPLE, 0 },
};

// Burning Fight (NGM-018 ~ NGH-018)
static const ROMEntry burningf_roms[] = {
    { "018-p1.p1",    0x00080000, 0x4092c8db, ROMType::PROGRAM, 0 },
    { "018-s1.s1",    0x00020000, 0x6799ea0d, ROMType::TEXT, 0 },
    { "018-c1.c1",    0x00100000, 0x25a25e9b, ROMType::SPRITE, 0 },
    { "018-c2.c2",    0x00100000, 0xd4378876, ROMType::SPRITE, 0 },
    { "018-c3.c3",    0x00100000, 0x862b60da, ROMType::SPRITE, 0 },
    { "018-c4.c4",    0x00100000, 0xe2e0aff7, ROMType::SPRITE, 0 },
    { "018-m1.m1",    0x00020000, 0x0c939ee2, ROMType::SOUND_PROGRAM, 0 },
    { "018-v1.v1",    0x00100000, 0x508c9ffc, ROMType::SOUND_SAMPLE, 0 },
    { "018-v2.v2",    0x00100000, 0x854ef277, ROMType::SOUND_SAMPLE, 0 },
};

// Captain Tomaday
static const ROMEntry ctomaday_roms[] = {
    { "249-p1.p1",    0x00200000, 0xc9386118, ROMType::PROGRAM, 0 },
    { "249-s1.s1",    0x00020000, 0xdc9eb372, ROMType::TEXT, 0 },
    { "249-c1.c1",    0x00400000, 0x041fb8ee, ROMType::SPRITE, 0 },
    { "249-c2.c2",    0x00400000, 0x74f3cdf4, ROMType::SPRITE, 0 },
    { "249-m1.m1",    0x00020000, 0x80328a47, ROMType::SOUND_PROGRAM, 0 },
    { "249-v1.v1",    0x00400000, 0xde7c8f27, ROMType::SOUND_SAMPLE, 0 },
    { "249-v2.v2",    0x00100000, 0xc8e40119, ROMType::SOUND_SAMPLE, 0 },
};

// Chibi Maruko-chan: Maruko Deluxe Quiz
static const ROMEntry marukodq_roms[] = {
    { "206-p1.p1",    0x00100000, 0xc33ed21e, ROMType::PROGRAM, 0 },
    { "206-s1.s1",    0x00020000, 0xf0b68780, ROMType::TEXT, 0 },
    { "206-c1.c1",    0x00400000, 0x846e4e8e, ROMType::SPRITE, 0 },
    { "206-c2.c2",    0x00400000, 0x1cba876d, ROMType::SPRITE, 0 },
    { "206-c3.c3",    0x00100000, 0x79aa2b48, ROMType::SPRITE, 0 },
    { "206-c4.c4",    0x00100000, 0x55e1314d, ROMType::SPRITE, 0 },
    { "206-m1.m1",    0x00020000, 0x0e22902e, ROMType::SOUND_PROGRAM, 0 },
    { "206-v1.v1",    0x00200000, 0x5385eca8, ROMType::SOUND_SAMPLE, 0 },
    { "206-v2.v2",    0x00200000, 0xf8c55404, ROMType::SOUND_SAMPLE, 0 },
};

// Choutetsu Brikin'ger / Iron Clad (prototype)
static const ROMEntry ironclad_roms[] = {
    { "proto_220-p1.p1",    0x00200000, 0x62a942c6, ROMType::PROGRAM, 0 },
    { "proto_220-s1.s1",    0x00020000, 0x372fe217, ROMType::TEXT, 0 },
    { "proto_220-c1.c1",    0x00400000, 0x9aa2b7dc, ROMType::SPRITE, 0 },
    { "proto_220-c2.c2",    0x00400000, 0x8a2ad708, ROMType::SPRITE, 0 },
    { "proto_220-c3.c3",    0x00400000, 0xd67fb15a, ROMType::SPRITE, 0 },
    { "proto_220-c4.c4",    0x00400000, 0xe73ea38b, ROMType::SPRITE, 0 },
    { "proto_220-m1.m1",    0x00020000, 0x3a08bb63, ROMType::SOUND_PROGRAM, 0 },
    { "proto_220-v1.v1",    0x00400000, 0x8f30a215, ROMType::SOUND_SAMPLE, 0 },
};

// Crossed Swords (ALM-002 ~ ALH-002)
static const ROMEntry crsword_roms[] = {
    { "037-p1.p1",    0x00080000, 0xe7f2553c, ROMType::PROGRAM, 0 },
    { "037-s1.s1",    0x00020000, 0x74651f27, ROMType::TEXT, 0 },
    { "037-c1.c1",    0x00100000, 0x09df6892, ROMType::SPRITE, 0 },
    { "037-c2.c2",    0x00100000, 0xac122a78, ROMType::SPRITE, 0 },
    { "037-c3.c3",    0x00100000, 0x9d7ed1ca, ROMType::SPRITE, 0 },
    { "037-c4.c4",    0x00100000, 0x4a24395d, ROMType::SPRITE, 0 },
    { "037-m1.m1",    0x00020000, 0x9504b2c6, ROMType::SOUND_PROGRAM, 0 },
    { "037-v1.v1",    0x00100000, 0x61fedf65, ROMType::SOUND_SAMPLE, 0 },
};

// Crossed Swords 2 (bootleg of CD version)
static const ROMEntry crswd2bl_roms[] = {
    { "054-p1.p1",    0x00200000, 0x64836147, ROMType::PROGRAM, 0 },
    { "054-s1.s1",    0x00020000, 0x22e02ddd, ROMType::TEXT, 0 },
    { "054-c1.c1",    0x00400000, 0x8221b712, ROMType::SPRITE, 0 },
    { "054-c2.c2",    0x00400000, 0xd6c6183d, ROMType::SPRITE, 0 },
    { "054-m1.m1",    0x00020000, 0x63e28343, ROMType::SOUND_PROGRAM, 0 },
    { "054-v1.v1",    0x00200000, 0x22d4b93b, ROMType::SOUND_SAMPLE, 0 },
};

// Cyber-Lip (NGM-010)
static const ROMEntry cyberlip_roms[] = {
    { "010-p1.p1",    0x00080000, 0x69a6b42d, ROMType::PROGRAM, 0 },
    { "010-s1.s1",    0x00020000, 0x79a35264, ROMType::TEXT, 0 },
    { "010-c1.c1",    0x00080000, 0x8bba5113, ROMType::SPRITE, 0 },
    { "010-c2.c2",    0x00080000, 0xcbf66432, ROMType::SPRITE, 0 },
    { "010-c3.c3",    0x00080000, 0xe4f86efc, ROMType::SPRITE, 0 },
    { "010-c4.c4",    0x00080000, 0xf7be4674, ROMType::SPRITE, 0 },
    { "010-c5.c5",    0x00080000, 0xe8076da0, ROMType::SPRITE, 0 },
    { "010-c6.c6",    0x00080000, 0xc495c567, ROMType::SPRITE, 0 },
    { "010-m1.m1",    0x00020000, 0x8be3a078, ROMType::SOUND_PROGRAM, 0 },
    { "010-v11.v11",  0x00080000, 0x90224d22, ROMType::SOUND_SAMPLE, 0 },
    { "010-v12.v12",  0x00080000, 0xa0cf1834, ROMType::SOUND_SAMPLE, 0 },
    { "010-v13.v13",  0x00080000, 0xae38bc84, ROMType::SOUND_SAMPLE, 0 },
    { "010-v14.v14",  0x00080000, 0x70899bd2, ROMType::SOUND_SAMPLE, 0 },
    { "010-v21.v21",  0x00080000, 0x586f4cb2, ROMType::SOUND_SAMPLE, 0 },
};

// Double Dragon (Neo-Geo)
static const ROMEntry doubledr_roms[] = {
    { "082-p1.p1",    0x00200000, 0x34ab832a, ROMType::PROGRAM, 0 },
    { "082-s1.s1",    0x00020000, 0xbef995c5, ROMType::TEXT, 0 },
    { "082-c1.c1",    0x00200000, 0xb478c725, ROMType::SPRITE, 0 },
    { "082-c2.c2",    0x00200000, 0x2857da32, ROMType::SPRITE, 0 },
    { "082-c3.c3",    0x00200000, 0x8b0d378e, ROMType::SPRITE, 0 },
    { "082-c4.c4",    0x00200000, 0xc7d2f596, ROMType::SPRITE, 0 },
    { "082-c5.c5",    0x00200000, 0xec87bff6, ROMType::SPRITE, 0 },
    { "082-c6.c6",    0x00200000, 0x844a8a11, ROMType::SPRITE, 0 },
    { "082-c7.c7",    0x00100000, 0x727c4d02, ROMType::SPRITE, 0 },
    { "082-c8.c8",    0x00100000, 0x69a5fa37, ROMType::SPRITE, 0 },
    { "082-m1.m1",    0x00020000, 0x10b144de, ROMType::SOUND_PROGRAM, 0 },
    { "082-v1.v1",    0x00200000, 0xcc1128e4, ROMType::SOUND_SAMPLE, 0 },
    { "082-v2.v2",    0x00200000, 0xc3ff5554, ROMType::SOUND_SAMPLE, 0 },
};

// Eight Man (NGM-025 ~ NGH-025)
static const ROMEntry eightman_roms[] = {
    { "025-p1.p1",    0x00080000, 0x43344cb0, ROMType::PROGRAM, 0 },
    { "025-s1.s1",    0x00020000, 0xa402202b, ROMType::TEXT, 0 },
    { "025-c1.c1",    0x00100000, 0x555e16a4, ROMType::SPRITE, 0 },
    { "025-c2.c2",    0x00100000, 0xe1ee51c3, ROMType::SPRITE, 0 },
    { "025-c3.c3",    0x00080000, 0x0923d5b0, ROMType::SPRITE, 0 },
    { "025-c4.c4",    0x00080000, 0xe3eca67b, ROMType::SPRITE, 0 },
    { "025-m1.m1",    0x00020000, 0x9927034c, ROMType::SOUND_PROGRAM, 0 },
    { "025-v1.v1",    0x00100000, 0x4558558a, ROMType::SOUND_SAMPLE, 0 },
    { "025-v2.v2",    0x00100000, 0xc5e052e9, ROMType::SOUND_SAMPLE, 0 },
};

// Far East of Eden - Kabuki Klash / Tengai Makyou - Shin Den
static const ROMEntry kabukikl_roms[] = {
    { "092-p1.p1",    0x00200000, 0x28ec9b77, ROMType::PROGRAM, 0 },
    { "092-s1.s1",    0x00020000, 0xa3d68ee2, ROMType::TEXT, 0 },
    { "092-c1.c1",    0x00400000, 0x2a9fab01, ROMType::SPRITE, 0 },
    { "092-c2.c2",    0x00400000, 0x6d2bac02, ROMType::SPRITE, 0 },
    { "092-c3.c3",    0x00400000, 0x5da735d6, ROMType::SPRITE, 0 },
    { "092-c4.c4",    0x00400000, 0xde07f997, ROMType::SPRITE, 0 },
    { "092-m1.m1",    0x00020000, 0x91957ef6, ROMType::SOUND_PROGRAM, 0 },
    { "092-v1.v1",    0x00200000, 0x69e90596, ROMType::SOUND_SAMPLE, 0 },
    { "092-v2.v2",    0x00200000, 0x7abdb75d, ROMType::SOUND_SAMPLE, 0 },
    { "092-v3.v3",    0x00200000, 0xeccc98d3, ROMType::SOUND_SAMPLE, 0 },
    { "092-v4.v4",    0x00100000, 0xa7c9c949, ROMType::SOUND_SAMPLE, 0 },
};

// Fatal Fury - King of Fighters / Garou Densetsu - Shukumei no Tatakai (NGM-033 ~ NGH-033)
static const ROMEntry fatfury1_roms[] = {
    { "033-p1.p1",    0x00080000, 0x47ebdc2f, ROMType::PROGRAM, 0 },
    { "033-p2.p2",    0x00020000, 0xc473af1c, ROMType::PROGRAM, 0 },
    { "033-s1.s1",    0x00020000, 0x3c3bdf8c, ROMType::TEXT, 0 },
    { "033-c1.c1",    0x00100000, 0x74317e54, ROMType::SPRITE, 0 },
    { "033-c2.c2",    0x00100000, 0x5bb952f3, ROMType::SPRITE, 0 },
    { "033-c3.c3",    0x00100000, 0x9b714a7c, ROMType::SPRITE, 0 },
    { "033-c4.c4",    0x00100000, 0x9397476a, ROMType::SPRITE, 0 },
    { "033-m1.m1",    0x00020000, 0x5be10ffd, ROMType::SOUND_PROGRAM, 0 },
    { "033-v1.v1",    0x00100000, 0x212fd20d, ROMType::SOUND_SAMPLE, 0 },
    { "033-v2.v2",    0x00100000, 0xfa2ae47f, ROMType::SOUND_SAMPLE, 0 },
};

// Fatal Fury 2 / Garou Densetsu 2 - Arata-naru Tatakai (NGM-047 ~ NGH-047)
static const ROMEntry fatfury2_roms[] = {
    { "047-p1.p1",    0x00100000, 0xecfdbb69, ROMType::PROGRAM, 0 },
    { "047-s1.s1",    0x00020000, 0xd7dbbf39, ROMType::TEXT, 0 },
    { "047-c1.c1",    0x00200000, 0xf72a939e, ROMType::SPRITE, 0 },
    { "047-c2.c2",    0x00200000, 0x05119a0d, ROMType::SPRITE, 0 },
    { "047-c3.c3",    0x00200000, 0x01e00738, ROMType::SPRITE, 0 },
    { "047-c4.c4",    0x00200000, 0x9fe27432, ROMType::SPRITE, 0 },
    { "047-m1.m1",    0x00020000, 0x820b0ba7, ROMType::SOUND_PROGRAM, 0 },
    { "047-v1.v1",    0x00200000, 0xd9d00784, ROMType::SOUND_SAMPLE, 0 },
    { "047-v2.v2",    0x00200000, 0x2c9a4b33, ROMType::SOUND_SAMPLE, 0 },
};

// Fatal Fury 3 - Road to the Final Victory / Garou Densetsu 3 - Haruka-naru Tatakai (NGM-069 ~ NGH-069)
static const ROMEntry fatfury3_roms[] = {
    { "069-p1.p1",    0x00100000, 0xa8bcfbbc, ROMType::PROGRAM, 0 },
    { "069-sp2.sp2",  0x00200000, 0xdbe963ed, ROMType::PROGRAM, 0 },
    { "069-s1.s1",    0x00020000, 0x0b33a800, ROMType::TEXT, 0 },
    { "069-c1.c1",    0x00400000, 0xe302f93c, ROMType::SPRITE, 0 },
    { "069-c2.c2",    0x00400000, 0x1053a455, ROMType::SPRITE, 0 },
    { "069-c3.c3",    0x00400000, 0x1c0fde2f, ROMType::SPRITE, 0 },
    { "069-c4.c4",    0x00400000, 0xa25fc3d0, ROMType::SPRITE, 0 },
    { "069-c5.c5",    0x00200000, 0xb3ec6fa6, ROMType::SPRITE, 0 },
    { "069-c6.c6",    0x00200000, 0x69210441, ROMType::SPRITE, 0 },
    { "069-m1.m1",    0x00020000, 0xfce72926, ROMType::SOUND_PROGRAM, 0 },
    { "069-v1.v1",    0x00400000, 0x2bdbd4db, ROMType::SOUND_SAMPLE, 0 },
    { "069-v2.v2",    0x00400000, 0xa698a487, ROMType::SOUND_SAMPLE, 0 },
    { "069-v3.v3",    0x00200000, 0x581c5304, ROMType::SOUND_SAMPLE, 0 },
};

// Fatal Fury Special / Garou Densetsu Special (NGM-058 ~ NGH-058, set 1)
static const ROMEntry fatfursp_roms[] = {
    { "058-p1.p1",    0x00100000, 0x2f585ba2, ROMType::PROGRAM, 0 },
    { "058-p2.sp2",   0x00080000, 0xd7c71a6b, ROMType::PROGRAM, 0 },
    { "058-s1.s1",    0x00020000, 0x2df03197, ROMType::TEXT, 0 },
    { "058-c1.c1",    0x00200000, 0x044ab13c, ROMType::SPRITE, 0 },
    { "058-c2.c2",    0x00200000, 0x11e6bf96, ROMType::SPRITE, 0 },
    { "058-c3.c3",    0x00200000, 0x6f7938d5, ROMType::SPRITE, 0 },
    { "058-c4.c4",    0x00200000, 0x4ad066ff, ROMType::SPRITE, 0 },
    { "058-c5.c5",    0x00200000, 0x49c5e0bf, ROMType::SPRITE, 0 },
    { "058-c6.c6",    0x00200000, 0x8ff1f43d, ROMType::SPRITE, 0 },
    { "058-m1.m1",    0x00020000, 0xccc5186e, ROMType::SOUND_PROGRAM, 0 },
    { "058-v1.v1",    0x00200000, 0x55d7ce84, ROMType::SOUND_SAMPLE, 0 },
    { "058-v2.v2",    0x00200000, 0xee080b10, ROMType::SOUND_SAMPLE, 0 },
    { "058-v3.v3",    0x00100000, 0xf9eb3d4a, ROMType::SOUND_SAMPLE, 0 },
};

// Fight Fever / Wang Jung Wang (set 1)
static const ROMEntry fightfev_roms[] = {
    { "060-p1.p1",    0x00100000, 0x2a104b50, ROMType::PROGRAM, 0 },
    { "060-s1.s1",    0x00020000, 0xd62a72e9, ROMType::TEXT, 0 },
    { "060-c1.c1",    0x00200000, 0x8908fff9, ROMType::SPRITE, 0 },
    { "060-c2.c2",    0x00200000, 0xc6649492, ROMType::SPRITE, 0 },
    { "060-c3.c3",    0x00200000, 0x0956b437, ROMType::SPRITE, 0 },
    { "060-c4.c4",    0x00200000, 0x026f3b62, ROMType::SPRITE, 0 },
    { "060-m1.m1",    0x00020000, 0x0b7c4e65, ROMType::SOUND_PROGRAM, 0 },
    { "060-v1.v1",    0x00200000, 0xf417c215, ROMType::SOUND_SAMPLE, 0 },
    { "060-v2.v2",    0x00100000, 0xefcff7cf, ROMType::SOUND_SAMPLE, 0 },
};

// Football Frenzy (NGM-034 ~ NGH-034)
static const ROMEntry fbfrenzy_roms[] = {
    { "034-p1.p1",    0x00080000, 0xcdef6b19, ROMType::PROGRAM, 0 },
    { "034-s1.s1",    0x00020000, 0x8472ed44, ROMType::TEXT, 0 },
    { "034-c1.c1",    0x00100000, 0x91c56e78, ROMType::SPRITE, 0 },
    { "034-c2.c2",    0x00100000, 0x9743ea2f, ROMType::SPRITE, 0 },
    { "034-c3.c3",    0x00080000, 0xe5aa65f5, ROMType::SPRITE, 0 },
    { "034-c4.c4",    0x00080000, 0x0eb138cc, ROMType::SPRITE, 0 },
    { "034-m1.m1",    0x00020000, 0xf41b16b8, ROMType::SOUND_PROGRAM, 0 },
    { "034-v1.v1",    0x00100000, 0x50c9d0dd, ROMType::SOUND_SAMPLE, 0 },
    { "034-v2.v2",    0x00100000, 0x5aa15686, ROMType::SOUND_SAMPLE, 0 },
};

// Galaxy Fight - Universal Warriors
static const ROMEntry galaxyfg_roms[] = {
    { "078-p1.p1",    0x00200000, 0x45906309, ROMType::PROGRAM, 0 },
    { "078-s1.s1",    0x00020000, 0x72f8923e, ROMType::TEXT, 0 },
    { "078-c1.c1",    0x00200000, 0xc890c7c0, ROMType::SPRITE, 0 },
    { "078-c2.c2",    0x00200000, 0xb6d25419, ROMType::SPRITE, 0 },
    { "078-c3.c3",    0x00200000, 0x9d87e761, ROMType::SPRITE, 0 },
    { "078-c4.c4",    0x00200000, 0x765d7cb8, ROMType::SPRITE, 0 },
    { "078-c5.c5",    0x00200000, 0xe6b77e6a, ROMType::SPRITE, 0 },
    { "078-c6.c6",    0x00200000, 0xd779a181, ROMType::SPRITE, 0 },
    { "078-c7.c7",    0x00100000, 0x4f27d580, ROMType::SPRITE, 0 },
    { "078-c8.c8",    0x00100000, 0x0a7cc0d8, ROMType::SPRITE, 0 },
    { "078-m1.m1",    0x00020000, 0x8e9e3b10, ROMType::SOUND_PROGRAM, 0 },
    { "078-v1.v1",    0x00200000, 0xe3b735ac, ROMType::SOUND_SAMPLE, 0 },
    { "078-v2.v2",    0x00200000, 0x6a8e78c2, ROMType::SOUND_SAMPLE, 0 },
    { "078-v3.v3",    0x00100000, 0x70bca656, ROMType::SOUND_SAMPLE, 0 },
};

// Ganryu / Musashi Ganryuki
static const ROMEntry ganryu_roms[] = {
    { "252-p1.p1",    0x00200000, 0x4b8ac4fb, ROMType::PROGRAM, 0 },
    { "252-c1.c1",    0x00800000, 0x50ee7882, ROMType::SPRITE, 0 },
    { "252-c2.c2",    0x00800000, 0x62585474, ROMType::SPRITE, 0 },
    { "252-m1.m1",    0x00020000, 0x30cc4099, ROMType::SOUND_PROGRAM, 0 },
    { "252-v1.v1",    0x00400000, 0xe5946733, ROMType::SOUND_SAMPLE, 0 },
};

// Garou - Mark of the Wolves (NGM-2530)
static const ROMEntry garou_roms[] = {
    { "kf.neo-sma",   0x00040000, 0x98bc93dc, ROMType::PROGRAM, 0 },
    { "253-ep1.p1",   0x00200000, 0xea3171a4, ROMType::PROGRAM, 0 },
    { "253-ep2.p2",   0x00200000, 0x382f704b, ROMType::PROGRAM, 0 },
    { "253-ep3.p3",   0x00200000, 0xe395bfdd, ROMType::PROGRAM, 0 },
    { "253-ep4.p4",   0x00200000, 0xda92c08e, ROMType::PROGRAM, 0 },
    { "253-c1.c1",    0x00800000, 0x0603e046, ROMType::SPRITE, 0 },
    { "253-c2.c2",    0x00800000, 0x0917d2a4, ROMType::SPRITE, 0 },
    { "253-c3.c3",    0x00800000, 0x6737c92d, ROMType::SPRITE, 0 },
    { "253-c4.c4",    0x00800000, 0x5ba92ec6, ROMType::SPRITE, 0 },
    { "253-c5.c5",    0x00800000, 0x3eab5557, ROMType::SPRITE, 0 },
    { "253-c6.c6",    0x00800000, 0x308d098b, ROMType::SPRITE, 0 },
    { "253-c7.c7",    0x00800000, 0xc0e995ae, ROMType::SPRITE, 0 },
    { "253-c8.c8",    0x00800000, 0x21a11303, ROMType::SPRITE, 0 },
    { "253-m1.m1",    0x00040000, 0x36a806be, ROMType::SOUND_PROGRAM, 0 },
    { "253-v1.v1",    0x00400000, 0x263e388c, ROMType::SOUND_SAMPLE, 0 },
    { "253-v2.v2",    0x00400000, 0x2c6bc7be, ROMType::SOUND_SAMPLE, 0 },
    { "253-v3.v3",    0x00400000, 0x0425b27d, ROMType::SOUND_SAMPLE, 0 },
    { "253-v4.v4",    0x00400000, 0xa54be8a9, ROMType::SOUND_SAMPLE, 0 },
};

// Ghost Pilots (NGM-020 ~ NGH-020)
static const ROMEntry gpilots_roms[] = {
    { "020-p1.p1",    0x00080000, 0xe6f2fe64, ROMType::PROGRAM, 0 },
    { "020-p2.p2",    0x00020000, 0xedcb22ac, ROMType::PROGRAM, 0 },
    { "020-s1.s1",    0x00020000, 0xa6d83d53, ROMType::TEXT, 0 },
    { "020-c1.c1",    0x00100000, 0xbd6fe78e, ROMType::SPRITE, 0 },
    { "020-c2.c2",    0x00100000, 0x5f4a925c, ROMType::SPRITE, 0 },
    { "020-c3.c3",    0x00100000, 0xd1e42fd0, ROMType::SPRITE, 0 },
    { "020-c4.c4",    0x00100000, 0xedde439b, ROMType::SPRITE, 0 },
    { "020-m1.m1",    0x00020000, 0x48409377, ROMType::SOUND_PROGRAM, 0 },
    { "020-v11.v11",  0x00100000, 0x1b526c8b, ROMType::SOUND_SAMPLE, 0 },
    { "020-v12.v12",  0x00080000, 0x4a9e6f03, ROMType::SOUND_SAMPLE, 0 },
    { "020-v21.v21",  0x00080000, 0x7abf113d, ROMType::SOUND_SAMPLE, 0 },
};

// Ghostlop (prototype)
static const ROMEntry ghostlop_roms[] = {
    { "proto_228-p1.p1",    0x00100000, 0x6033172e, ROMType::PROGRAM, 0 },
    { "proto_228-s1.s1",    0x00020000, 0x83c24e81, ROMType::TEXT, 0 },
    { "proto_228-c1.c1",    0x00400000, 0xbfc99efe, ROMType::SPRITE, 0 },
    { "proto_228-c2.c2",    0x00400000, 0x69788082, ROMType::SPRITE, 0 },
    { "proto_228-m1.m1",    0x00020000, 0xfd833b33, ROMType::SOUND_PROGRAM, 0 },
    { "proto_228-v1.v1",    0x00200000, 0xc603fce6, ROMType::SOUND_SAMPLE, 0 },
};

// Goal! Goal! Goal!
static const ROMEntry goalx3_roms[] = {
    { "209-p1.p1",    0x00200000, 0x2a019a79, ROMType::PROGRAM, 0 },
    { "209-s1.s1",    0x00020000, 0xc0eaad86, ROMType::TEXT, 0 },
    { "209-c1.c1",    0x00400000, 0xb49d980e, ROMType::SPRITE, 0 },
    { "209-c2.c2",    0x00400000, 0x5649b015, ROMType::SPRITE, 0 },
    { "209-c3.c3",    0x00100000, 0x5f91bace, ROMType::SPRITE, 0 },
    { "209-c4.c4",    0x00100000, 0x1e9f76f2, ROMType::SPRITE, 0 },
    { "209-m1.m1",    0x00020000, 0xcd758325, ROMType::SOUND_PROGRAM, 0 },
    { "209-v1.v1",    0x00200000, 0xef214212, ROMType::SOUND_SAMPLE, 0 },
};

// Gururin
static const ROMEntry gururin_roms[] = {
    { "067-p1.p1",    0x00080000, 0x4cea8a49, ROMType::PROGRAM, 0 },
    { "067-s1.s1",    0x00020000, 0xb119e1eb, ROMType::TEXT, 0 },
    { "067-c1.c1",    0x00200000, 0x35866126, ROMType::SPRITE, 0 },
    { "067-c2.c2",    0x00200000, 0x9db64084, ROMType::SPRITE, 0 },
    { "067-m1.m1",    0x00020000, 0x9e3c6328, ROMType::SOUND_PROGRAM, 0 },
    { "067-v1.v1",    0x00080000, 0xcf23afd0, ROMType::SOUND_SAMPLE, 0 },
};

// Idol Mahjong Final Romance 2 (Neo-Geo, bootleg of CD version)
static const ROMEntry froman2b_roms[] = {
    { "098.p1",    0x00080000, 0x09675541, ROMType::PROGRAM, 0 },
    { "098.s1",    0x00020000, 0x0e6a7c73, ROMType::TEXT, 0 },
    { "098.c1",    0x00400000, 0x29148bf7, ROMType::SPRITE, 0 },
    { "098.c2",    0x00400000, 0x226b1263, ROMType::SPRITE, 0 },
    { "098.m1",    0x00020000, 0xda4878cf, ROMType::SOUND_PROGRAM, 0 },
    { "098.v1",    0x00100000, 0x6f8ccddc, ROMType::SOUND_SAMPLE, 0 },
};

// Janshin Densetsu - Quest of Jongmaster
static const ROMEntry janshin_roms[] = {
    { "048-p1.p1",    0x00100000, 0xfa818cbb, ROMType::PROGRAM, 0 },
    { "048-s1.s1",    0x00020000, 0x8285b25a, ROMType::TEXT, 0 },
    { "048-c1.c1",    0x00200000, 0x3fa890e9, ROMType::SPRITE, 0 },
    { "048-c2.c2",    0x00200000, 0x59c48ad8, ROMType::SPRITE, 0 },
    { "048-m1.m1",    0x00020000, 0x310467c7, ROMType::SOUND_PROGRAM, 0 },
    { "048-v1.v1",    0x00200000, 0xf1947d2b, ROMType::SOUND_SAMPLE, 0 },
};

// Jockey Grand Prix (set 1)
static const ROMEntry jockeygp_roms[] = {
    { "008-epr.p1",   0x00100000, 0x2fb7f388, ROMType::PROGRAM, 0 },
    { "008-c1.c1",    0x00800000, 0xa9acbf18, ROMType::SPRITE, 0 },
    { "008-c2.c2",    0x00800000, 0x6289eef9, ROMType::SPRITE, 0 },
    { "008-mg1.m1",   0x00080000, 0xd163c690, ROMType::SOUND_PROGRAM, 0 },
    { "008-v1.v1",    0x00200000, 0x443eadba, ROMType::SOUND_SAMPLE, 0 },
};

// Karnov's Revenge / Fighter's History Dynamite
static const ROMEntry karnovr_roms[] = {
    { "066-p1.p1",    0x00100000, 0x8c86fd22, ROMType::PROGRAM, 0 },
    { "066-s1.s1",    0x00020000, 0xbae5d5e5, ROMType::TEXT, 0 },
    { "066-c1.c1",    0x00200000, 0x09dfe061, ROMType::SPRITE, 0 },
    { "066-c2.c2",    0x00200000, 0xe0f6682a, ROMType::SPRITE, 0 },
    { "066-c3.c3",    0x00200000, 0xa673b4f7, ROMType::SPRITE, 0 },
    { "066-c4.c4",    0x00200000, 0xcb3dc5f4, ROMType::SPRITE, 0 },
    { "066-c5.c5",    0x00200000, 0x9a28785d, ROMType::SPRITE, 0 },
    { "066-c6.c6",    0x00200000, 0xc15c01ed, ROMType::SPRITE, 0 },
    { "066-m1.m1",    0x00020000, 0x030beae4, ROMType::SOUND_PROGRAM, 0 },
    { "066-v1.v1",    0x00200000, 0x0b7ea37a, ROMType::SOUND_SAMPLE, 0 },
};

// King of the Monsters (set 1)
static const ROMEntry kotm_roms[] = {
    { "016-p1.p1",    0x00080000, 0x1b818731, ROMType::PROGRAM, 0 },
    { "016-p2.p2",    0x00020000, 0x12afdc2b, ROMType::PROGRAM, 0 },
    { "016-s1.s1",    0x00020000, 0x1a2eeeb3, ROMType::TEXT, 0 },
    { "016-c1.c1",    0x00100000, 0x71471c25, ROMType::SPRITE, 0 },
    { "016-c2.c2",    0x00100000, 0x320db048, ROMType::SPRITE, 0 },
    { "016-c3.c3",    0x00100000, 0x98de7995, ROMType::SPRITE, 0 },
    { "016-c4.c4",    0x00100000, 0x070506e2, ROMType::SPRITE, 0 },
    { "016-m1.m1",    0x00020000, 0x9da9ca10, ROMType::SOUND_PROGRAM, 0 },
    { "016-v1.v1",    0x00100000, 0x86c0a502, ROMType::SOUND_SAMPLE, 0 },
    { "016-v2.v2",    0x00100000, 0x5bc23ec5, ROMType::SOUND_SAMPLE, 0 },
};

// King of the Monsters 2 - The Next Thing (NGM-039 ~ NGH-039)
static const ROMEntry kotm2_roms[] = {
    { "039-p1.p1",    0x00080000, 0xb372d54c, ROMType::PROGRAM, 0 },
    { "039-p2.p2",    0x00080000, 0x28661afe, ROMType::PROGRAM, 0 },
    { "039-s1.s1",    0x00020000, 0x63ee053a, ROMType::TEXT, 0 },
    { "039-c1.c1",    0x00200000, 0x6d1c4aa9, ROMType::SPRITE, 0 },
    { "039-c2.c2",    0x00200000, 0xf7b75337, ROMType::SPRITE, 0 },
    { "039-c3.c3",    0x00080000, 0xbfc4f0b2, ROMType::SPRITE, 0 },
    { "039-c4.c4",    0x00080000, 0x81c9c250, ROMType::SPRITE, 0 },
    { "039-m1.m1",    0x00020000, 0x0c5b2ad5, ROMType::SOUND_PROGRAM, 0 },
    { "039-v2.v2",    0x00200000, 0x86d34b25, ROMType::SOUND_SAMPLE, 0 },
    { "039-v4.v4",    0x00100000, 0x8fa62a0b, ROMType::SOUND_SAMPLE, 0 },
};

// Kizuna Encounter - Super Tag Battle / Fu'un Super Tag Battle
static const ROMEntry kizuna_roms[] = {
    { "216-p1.p1",    0x00200000, 0x75d2b3de, ROMType::PROGRAM, 0 },
    { "216-s1.s1",    0x00020000, 0xefdc72d7, ROMType::TEXT, 0 },
    { "059-c1.c1",    0x00200000, 0x763ba611, ROMType::SPRITE, 0 },
    { "059-c2.c2",    0x00200000, 0xe05e8ca6, ROMType::SPRITE, 0 },
    { "216-c3.c3",    0x00400000, 0x665c9f16, ROMType::SPRITE, 0 },
    { "216-c4.c4",    0x00400000, 0x7f5d03db, ROMType::SPRITE, 0 },
    { "059-c5.c5",    0x00200000, 0x59013f9e, ROMType::SPRITE, 0 },
    { "059-c6.c6",    0x00200000, 0x1c8d5def, ROMType::SPRITE, 0 },
    { "059-c7.c7",    0x00200000, 0xc88f7035, ROMType::SPRITE, 0 },
    { "059-c8.c8",    0x00200000, 0x484ce3ba, ROMType::SPRITE, 0 },
    { "216-m1.m1",    0x00020000, 0x1b096820, ROMType::SOUND_PROGRAM, 0 },
    { "059-v1.v1",    0x00200000, 0x530c50fd, ROMType::SOUND_SAMPLE, 0 },
    { "216-v2.v2",    0x00200000, 0x03667a8d, ROMType::SOUND_SAMPLE, 0 },
    { "059-v3.v3",    0x00200000, 0x7038c2f9, ROMType::SOUND_SAMPLE, 0 },
    { "216-v4.v4",    0x00200000, 0x31b99bd6, ROMType::SOUND_SAMPLE, 0 },
};

// Last Hope (bootleg AES to MVS conversion, no coin support)
static const ROMEntry lasthope_roms[] = {
    { "ngdt-300-p1.bin",    0x00100000, 0x3776a88f, ROMType::PROGRAM, 0 },
    { "ngdt-300-s1.bin",    0x00010000, 0x0c0ff9e6, ROMType::TEXT, 0 },
    { "ngdt-300-c1.bin",    0x00400000, 0x53ef41b5, ROMType::SPRITE, 0 },
    { "ngdt-300-c2.bin",    0x00400000, 0xf9b15ab3, ROMType::SPRITE, 0 },
    { "ngdt-300-c3.bin",    0x00400000, 0x50cc21cf, ROMType::SPRITE, 0 },
    { "ngdt-300-c4.bin",    0x00400000, 0x8486ad9e, ROMType::SPRITE, 0 },
    { "ngdt-300-m1.bin",    0x00020000, 0x113c870f, ROMType::SOUND_PROGRAM, 0 },
    { "ngdt-300-v1.bin",    0x00200000, 0xb765bafe, ROMType::SOUND_SAMPLE, 0 },
    { "ngdt-300-v2.bin",    0x00200000, 0x9fd0d559, ROMType::SOUND_SAMPLE, 0 },
    { "ngdt-300-v3.bin",    0x00200000, 0x6d5107e2, ROMType::SOUND_SAMPLE, 0 },
};

// Last Resort
static const ROMEntry lresort_roms[] = {
    { "024-p1.p1",    0x00080000, 0x89c4ab97, ROMType::PROGRAM, 0 },
    { "024-s1.s1",    0x00020000, 0x5cef5cc6, ROMType::TEXT, 0 },
    { "024-c1.c1",    0x00100000, 0x3617c2dc, ROMType::SPRITE, 0 },
    { "024-c2.c2",    0x00100000, 0x3f0a7fd8, ROMType::SPRITE, 0 },
    { "024-c3.c3",    0x00080000, 0xe9f745f8, ROMType::SPRITE, 0 },
    { "024-c4.c4",    0x00080000, 0x7382fefb, ROMType::SPRITE, 0 },
    { "024-m1.m1",    0x00020000, 0xcec19742, ROMType::SOUND_PROGRAM, 0 },
    { "024-v1.v1",    0x00100000, 0xefdfa063, ROMType::SOUND_SAMPLE, 0 },
    { "024-v2.v2",    0x00100000, 0x3c7997c0, ROMType::SOUND_SAMPLE, 0 },
};

// League Bowling (NGM-019 ~ NGH-019)
static const ROMEntry lbowling_roms[] = {
    { "019-p1.p1",    0x00080000, 0xa2de8445, ROMType::PROGRAM, 0 },
    { "019-s1.s1",    0x00020000, 0x5fcdc0ed, ROMType::TEXT, 0 },
    { "019-c1.c1",    0x00080000, 0x4ccdef18, ROMType::SPRITE, 0 },
    { "019-c2.c2",    0x00080000, 0xd4dd0802, ROMType::SPRITE, 0 },
    { "019-m1.m1",    0x00020000, 0xd568c17d, ROMType::SOUND_PROGRAM, 0 },
    { "019-v11.v11",  0x00080000, 0x0fb74872, ROMType::SOUND_SAMPLE, 0 },
    { "019-v12.v12",  0x00080000, 0x029faa57, ROMType::SOUND_SAMPLE, 0 },
    { "019-v21.v21",  0x00080000, 0x2efd5ada, ROMType::SOUND_SAMPLE, 0 },
};

// Legend of Success Joe / Ashita no Joe Densetsu
static const ROMEntry legendos_roms[] = {
    { "029-p1.p1",    0x00080000, 0x9d563f19, ROMType::PROGRAM, 0 },
    { "029-s1.s1",    0x00020000, 0xbcd502f0, ROMType::TEXT, 0 },
    { "029-c1.c1",    0x00100000, 0x2f5ab875, ROMType::SPRITE, 0 },
    { "029-c2.c2",    0x00100000, 0x318b2711, ROMType::SPRITE, 0 },
    { "029-c3.c3",    0x00100000, 0x6bc52cb2, ROMType::SPRITE, 0 },
    { "029-c4.c4",    0x00100000, 0x37ef298c, ROMType::SPRITE, 0 },
    { "029-m1.m1",    0x00020000, 0x6f2843f0, ROMType::SOUND_PROGRAM, 0 },
    { "029-v1.v1",    0x00100000, 0x85065452, ROMType::SOUND_SAMPLE, 0 },
};

// Magical Drop III
static const ROMEntry magdrop3_roms[] = {
    { "233-p1.p1",    0x00100000, 0x931e17fa, ROMType::PROGRAM, 0 },
    { "233-s1.s1",    0x00020000, 0x7399e68a, ROMType::TEXT, 0 },
    { "233-c1.c1",    0x00400000, 0x65e3f4c4, ROMType::SPRITE, 0 },
    { "233-c2.c2",    0x00400000, 0x35dea6c9, ROMType::SPRITE, 0 },
    { "233-c3.c3",    0x00400000, 0x0ba2c502, ROMType::SPRITE, 0 },
    { "233-c4.c4",    0x00400000, 0x70dbbd6d, ROMType::SPRITE, 0 },
    { "233-m1.m1",    0x00020000, 0x5beaf34e, ROMType::SOUND_PROGRAM, 0 },
    { "233-v1.v1",    0x00400000, 0x58839298, ROMType::SOUND_SAMPLE, 0 },
    { "233-v2.v2",    0x00080000, 0xd5e30df4, ROMType::SOUND_SAMPLE, 0 },
};

// Magical Drop II
static const ROMEntry magdrop2_roms[] = {
    { "221-p1.p1",    0x00080000, 0x7be82353, ROMType::PROGRAM, 0 },
    { "221-s1.s1",    0x00020000, 0x2a4063a3, ROMType::TEXT, 0 },
    { "221-c1.c1",    0x00400000, 0x1f862a14, ROMType::SPRITE, 0 },
    { "221-c2.c2",    0x00400000, 0x14b90536, ROMType::SPRITE, 0 },
    { "221-m1.m1",    0x00020000, 0xbddae628, ROMType::SOUND_PROGRAM, 0 },
    { "221-v1.v1",    0x00200000, 0x7e5e53e4, ROMType::SOUND_SAMPLE, 0 },
};

// Magician Lord (NGM-005)
static const ROMEntry maglord_roms[] = {
    { "005-pg1.p1",   0x00080000, 0xbd0a492d, ROMType::PROGRAM, 0 },
    { "005-s1.s1",    0x00020000, 0x1c5369a2, ROMType::TEXT, 0 },
    { "005-c1.c1",    0x00080000, 0x806aee34, ROMType::SPRITE, 0 },
    { "005-c2.c2",    0x00080000, 0x34aa9a86, ROMType::SPRITE, 0 },
    { "005-c3.c3",    0x00080000, 0xc4c2b926, ROMType::SPRITE, 0 },
    { "005-c4.c4",    0x00080000, 0x9c46dcf4, ROMType::SPRITE, 0 },
    { "005-c5.c5",    0x00080000, 0x69086dec, ROMType::SPRITE, 0 },
    { "005-c6.c6",    0x00080000, 0xab7ac142, ROMType::SPRITE, 0 },
    { "005-m1.m1",    0x00040000, 0x26259f0f, ROMType::SOUND_PROGRAM, 0 },
    { "005-v11.v11",  0x00080000, 0xcc0455fd, ROMType::SOUND_SAMPLE, 0 },
    { "005-v21.v21",  0x00080000, 0xf94ab5b7, ROMType::SOUND_SAMPLE, 0 },
    { "005-v22.v22",  0x00080000, 0x232cfd04, ROMType::SOUND_SAMPLE, 0 },
};

// Mahjong Kyo Retsuden (NGM-004 ~ NGH-004)
static const ROMEntry mahretsu_roms[] = {
    { "004-p1.p1",    0x00080000, 0xfc6f53db, ROMType::PROGRAM, 0 },
    { "004-s1.s1",    0x00020000, 0x2bd05a06, ROMType::TEXT, 0 },
    { "004-c1.c1",    0x00080000, 0xf1ae16bc, ROMType::SPRITE, 0 },
    { "004-c2.c2",    0x00080000, 0xbdc13520, ROMType::SPRITE, 0 },
    { "004-c3.c3",    0x00080000, 0x9c571a37, ROMType::SPRITE, 0 },
    { "004-c4.c4",    0x00080000, 0x7e81cb29, ROMType::SPRITE, 0 },
    { "004-m1.m1",    0x00020000, 0xc71fbb3b, ROMType::SOUND_PROGRAM, 0 },
    { "004-v11.v11",  0x00080000, 0xb2fb2153, ROMType::SOUND_SAMPLE, 0 },
    { "004-v12.v12",  0x00080000, 0x8503317b, ROMType::SOUND_SAMPLE, 0 },
    { "004-v21.v21",  0x00080000, 0x4999fb27, ROMType::SOUND_SAMPLE, 0 },
    { "004-v22.v22",  0x00080000, 0x776fa2a2, ROMType::SOUND_SAMPLE, 0 },
    { "004-v23.v23",  0x00080000, 0xb3e7eeea, ROMType::SOUND_SAMPLE, 0 },
};

// Matrimelee / Shin Gouketsuji Ichizoku Toukon (NGM-2660 ~ NGH-2660)
static const ROMEntry matrim_roms[] = {
    { "266-p1.p1",    0x00100000, 0x5d4c2dc7, ROMType::PROGRAM, 0 },
    { "266-p2.sp2",   0x00400000, 0xa14b1906, ROMType::PROGRAM, 0 },
    { "266-c1.c1",    0x00800000, 0x505f4e30, ROMType::SPRITE, 0 },
    { "266-c2.c2",    0x00800000, 0x3cb57482, ROMType::SPRITE, 0 },
    { "266-c3.c3",    0x00800000, 0xf1cc6ad0, ROMType::SPRITE, 0 },
    { "266-c4.c4",    0x00800000, 0x45b806b7, ROMType::SPRITE, 0 },
    { "266-c5.c5",    0x00800000, 0x9a15dd6b, ROMType::SPRITE, 0 },
    { "266-c6.c6",    0x00800000, 0x281cb939, ROMType::SPRITE, 0 },
    { "266-c7.c7",    0x00800000, 0x4b71f780, ROMType::SPRITE, 0 },
    { "266-c8.c8",    0x00800000, 0x29873d33, ROMType::SPRITE, 0 },
    { "266-m1.m1",    0x00020000, 0x456c3e6c, ROMType::SOUND_PROGRAM, 0 },
    { "266-v1.v1",    0x00800000, 0xa4f83690, ROMType::SOUND_SAMPLE, 0 },
    { "266-v2.v2",    0x00800000, 0xd0f69eda, ROMType::SOUND_SAMPLE, 0 },
};

// Metal Slug - Super Vehicle-001
static const ROMEntry mslug_roms[] = {
    { "201-p1.p1",    0x00200000, 0x08d8daa5, ROMType::PROGRAM, 0 },
    { "201-s1.s1",    0x00020000, 0x2f55958d, ROMType::TEXT, 0 },
    { "201-c1.c1",    0x00400000, 0x72813676, ROMType::SPRITE, 0 },
    { "201-c2.c2",    0x00400000, 0x96f62574, ROMType::SPRITE, 0 },
    { "201-c3.c3",    0x00400000, 0x5121456a, ROMType::SPRITE, 0 },
    { "201-c4.c4",    0x00400000, 0xf4ad59a3, ROMType::SPRITE, 0 },
    { "201-m1.m1",    0x00020000, 0xc28b3253, ROMType::SOUND_PROGRAM, 0 },
    { "201-v1.v1",    0x00400000, 0x23d22ed1, ROMType::SOUND_SAMPLE, 0 },
    { "201-v2.v2",    0x00400000, 0x472cf9db, ROMType::SOUND_SAMPLE, 0 },
};

// Metal Slug 2 - Super Vehicle-001/II (NGM-2410 ~ NGH-2410)
static const ROMEntry mslug2_roms[] = {
    { "241-p1.p1",    0x00100000, 0x2a53c5da, ROMType::PROGRAM, 0 },
    { "241-p2.sp2",   0x00200000, 0x38883f44, ROMType::PROGRAM, 0 },
    { "241-s1.s1",    0x00020000, 0xf3d32f0f, ROMType::TEXT, 0 },
    { "241-c1.c1",    0x00800000, 0x394b5e0d, ROMType::SPRITE, 0 },
    { "241-c2.c2",    0x00800000, 0xe5806221, ROMType::SPRITE, 0 },
    { "241-c3.c3",    0x00800000, 0x9f6bfa6f, ROMType::SPRITE, 0 },
    { "241-c4.c4",    0x00800000, 0x7d3e306f, ROMType::SPRITE, 0 },
    { "241-m1.m1",    0x00020000, 0x94520ebd, ROMType::SOUND_PROGRAM, 0 },
    { "241-v1.v1",    0x00400000, 0x99ec20e8, ROMType::SOUND_SAMPLE, 0 },
    { "241-v2.v2",    0x00400000, 0xecb16799, ROMType::SOUND_SAMPLE, 0 },
};

// Metal Slug 3 (Fully Decrypted)
static const ROMEntry mslug3fd_roms[] = {
    { "256-ph1.p1",    0x00100000, 0x9c42ca85, ROMType::PROGRAM, 0 },
    { "256-ph2.sp2",   0x00400000, 0x1f3d8ce8, ROMType::PROGRAM, 0 },
    { "256-s1d.s1",    0x00020000, 0x8458fff9, ROMType::TEXT, 0 },
    { "256-c1d.c1",    0x00800000, 0x3540398c, ROMType::SPRITE, 0 },
    { "256-c2d.c2",    0x00800000, 0xbdd220f0, ROMType::SPRITE, 0 },
    { "256-c3d.c3",    0x00800000, 0xbfaade82, ROMType::SPRITE, 0 },
    { "256-c4d.c4",    0x00800000, 0x1463add6, ROMType::SPRITE, 0 },
    { "256-c5d.c5",    0x00800000, 0x48ca7f28, ROMType::SPRITE, 0 },
    { "256-c6d.c6",    0x00800000, 0x806eb36f, ROMType::SPRITE, 0 },
    { "256-c7d.c7",    0x00800000, 0x9395b809, ROMType::SPRITE, 0 },
    { "256-c8d.c8",    0x00800000, 0xa369f9d4, ROMType::SPRITE, 0 },
    { "256-m1.m1",     0x00080000, 0xeaeec116, ROMType::SOUND_PROGRAM, 0 },
    { "256-v1.v1",     0x00400000, 0xf2690241, ROMType::SOUND_SAMPLE, 0 },
    { "256-v2.v2",     0x00400000, 0x7e2a10bd, ROMType::SOUND_SAMPLE, 0 },
    { "256-v3.v3",     0x00400000, 0x0eaec17c, ROMType::SOUND_SAMPLE, 0 },
    { "256-v4.v4",     0x00400000, 0x9b4b22d4, ROMType::SOUND_SAMPLE, 0 },
};

// Metal Slug 3 (NGM-2560)
static const ROMEntry mslug3_roms[] = {
    { "green.neo-sma", 0x00040000, 0x9cd55736, ROMType::PROGRAM, 0 },
    { "256-pg1.p1",    0x00400000, 0xb07edfd5, ROMType::PROGRAM, 0 },
    { "256-pg2.p2",    0x00400000, 0x6097c26b, ROMType::PROGRAM, 0 },
    { "256-c1.c1",     0x00800000, 0x5a79c34e, ROMType::SPRITE, 0 },
    { "256-c2.c2",     0x00800000, 0x944c362c, ROMType::SPRITE, 0 },
    { "256-c3.c3",     0x00800000, 0x6e69d36f, ROMType::SPRITE, 0 },
    { "256-c4.c4",     0x00800000, 0xb755b4eb, ROMType::SPRITE, 0 },
    { "256-c5.c5",     0x00800000, 0x7aacab47, ROMType::SPRITE, 0 },
    { "256-c6.c6",     0x00800000, 0xc698fd5d, ROMType::SPRITE, 0 },
    { "256-c7.c7",     0x00800000, 0xcfceddd2, ROMType::SPRITE, 0 },
    { "256-c8.c8",     0x00800000, 0x4d9be34c, ROMType::SPRITE, 0 },
    { "256-m1.m1",     0x00080000, 0xeaeec116, ROMType::SOUND_PROGRAM, 0 },
    { "256-v1.v1",     0x00400000, 0xf2690241, ROMType::SOUND_SAMPLE, 0 },
    { "256-v2.v2",     0x00400000, 0x7e2a10bd, ROMType::SOUND_SAMPLE, 0 },
    { "256-v3.v3",     0x00400000, 0x0eaec17c, ROMType::SOUND_SAMPLE, 0 },
    { "256-v4.v4",     0x00400000, 0x9b4b22d4, ROMType::SOUND_SAMPLE, 0 },
};

// Metal Slug 4 (Fully Decrypted)
static const ROMEntry mslug4fd_roms[] = {
    { "263-p1.p1",     0x00100000, 0x27e4def3, ROMType::PROGRAM, 0 },
    { "263-p2.sp2",    0x00400000, 0xfdb7aed8, ROMType::PROGRAM, 0 },
    { "263-s1d.s1",    0x00020000, 0xa9446774, ROMType::TEXT, 0 },
    { "263-c1d.c1",    0x00800000, 0xa75ffcde, ROMType::SPRITE, 0 },
    { "263-c2d.c2",    0x00800000, 0x5ab0d12b, ROMType::SPRITE, 0 },
    { "263-c3d.c3",    0x00800000, 0x61af560c, ROMType::SPRITE, 0 },
    { "263-c4d.c4",    0x00800000, 0xf2c544fd, ROMType::SPRITE, 0 },
    { "263-c5d.c5",    0x00800000, 0x84c66c44, ROMType::SPRITE, 0 },
    { "263-c6d.c6",    0x00800000, 0x5ed018ab, ROMType::SPRITE, 0 },
    { "263-m1d.m1",    0x00020000, 0xef5db532, ROMType::SOUND_PROGRAM, 0 },
    { "263-v1d.v1",    0x00400000, 0x8cb5a9ef, ROMType::SOUND_SAMPLE, 0 },
    { "263-v2d.v2",    0x00400000, 0x94217b1e, ROMType::SOUND_SAMPLE, 0 },
    { "263-v3d.v3",    0x00400000, 0x7616fcec, ROMType::SOUND_SAMPLE, 0 },
    { "263-v4d.v4",    0x00400000, 0xc5967f91, ROMType::SOUND_SAMPLE, 0 },
};

// Metal Slug 4 (NGM-2630)
static const ROMEntry mslug4_roms[] = {
    { "263-p1.p1",    0x00100000, 0x27e4def3, ROMType::PROGRAM, 0 },
    { "263-p2.sp2",   0x00400000, 0xfdb7aed8, ROMType::PROGRAM, 0 },
    { "263-c1.c1",    0x00800000, 0x84865f8a, ROMType::SPRITE, 0 },
    { "263-c2.c2",    0x00800000, 0x81df97f2, ROMType::SPRITE, 0 },
    { "263-c3.c3",    0x00800000, 0x1a343323, ROMType::SPRITE, 0 },
    { "263-c4.c4",    0x00800000, 0x942cfb44, ROMType::SPRITE, 0 },
    { "263-c5.c5",    0x00800000, 0xa748854f, ROMType::SPRITE, 0 },
    { "263-c6.c6",    0x00800000, 0x5c8ba116, ROMType::SPRITE, 0 },
    { "263-m1.m1",    0x00020000, 0x46ac8228, ROMType::SOUND_PROGRAM, 0 },
    { "263-v1.v1",    0x00800000, 0x01e9b9cd, ROMType::SOUND_SAMPLE, 0 },
    { "263-v2.v2",    0x00800000, 0x4ab2bf81, ROMType::SOUND_SAMPLE, 0 },
};

// Metal Slug 5 (Fully Decrypted)
static const ROMEntry mslug5fd_roms[] = {
    { "268-p1d.p1",    0x00100000, 0x24ae2e4d, ROMType::PROGRAM, 0 },
    { "268-p2d.sp2",   0x00400000, 0x768ee64a, ROMType::PROGRAM, 0 },
    { "268-s1d.s1",    0x00020000, 0x64952683, ROMType::TEXT, 0 },
    { "268-c1d.c1",    0x00800000, 0xe8239365, ROMType::SPRITE, 0 },
    { "268-c2d.c2",    0x00800000, 0x89b21d4c, ROMType::SPRITE, 0 },
    { "268-c3d.c3",    0x00800000, 0x3cda13a0, ROMType::SPRITE, 0 },
    { "268-c4d.c4",    0x00800000, 0x9c00160d, ROMType::SPRITE, 0 },
    { "268-c5d.c5",    0x00800000, 0x38754256, ROMType::SPRITE, 0 },
    { "268-c6d.c6",    0x00800000, 0x59d33e9c, ROMType::SPRITE, 0 },
    { "268-c7d.c7",    0x00800000, 0xc9f8c357, ROMType::SPRITE, 0 },
    { "268-c8d.c8",    0x00800000, 0xfafc3eb9, ROMType::SPRITE, 0 },
    { "268-m1d.m1",    0x00020000, 0x346d4a30, ROMType::SOUND_PROGRAM, 0 },
    { "268-v1d.v1",    0x00800000, 0x7ff6ca47, ROMType::SOUND_SAMPLE, 0 },
    { "268-v2d.v2",    0x00800000, 0x696cce3b, ROMType::SOUND_SAMPLE, 0 },
};

// Metal Slug 5 (NGM-2680)
static const ROMEntry mslug5_roms[] = {
    { "268-p1cr.p1",   0x00400000, 0xd0466792, ROMType::PROGRAM, 0 },
    { "268-p2cr.p2",   0x00400000, 0xfbf6b61e, ROMType::PROGRAM, 0 },
    { "268-c1c.c1",    0x00800000, 0xab7c389a, ROMType::SPRITE, 0 },
    { "268-c2c.c2",    0x00800000, 0x3560881b, ROMType::SPRITE, 0 },
    { "268-c3c.c3",    0x00800000, 0x3af955ea, ROMType::SPRITE, 0 },
    { "268-c4c.c4",    0x00800000, 0xc329c373, ROMType::SPRITE, 0 },
    { "268-c5c.c5",    0x00800000, 0x959c8177, ROMType::SPRITE, 0 },
    { "268-c6c.c6",    0x00800000, 0x010a831b, ROMType::SPRITE, 0 },
    { "268-c7c.c7",    0x00800000, 0x6d72a969, ROMType::SPRITE, 0 },
    { "268-c8c.c8",    0x00800000, 0x551d720e, ROMType::SPRITE, 0 },
    { "268-m1.m1",     0x00080000, 0x4a5a6e0e, ROMType::SOUND_PROGRAM, 0 },
    { "268-v1c.v1",    0x00800000, 0xae31d60c, ROMType::SOUND_SAMPLE, 0 },
    { "268-v2c.v2",    0x00800000, 0xc40613ed, ROMType::SOUND_SAMPLE, 0 },
};

// Metal Slug X - Super Vehicle-001 (NGM-2500 ~ NGH-2500)
static const ROMEntry mslugx_roms[] = {
    { "250-p1.p1",    0x00100000, 0x81f1f60b, ROMType::PROGRAM, 0 },
    { "250-p2.ep1",   0x00400000, 0x1fda2e12, ROMType::PROGRAM, 0 },
    { "250-s1.s1",    0x00020000, 0xfb6f441d, ROMType::TEXT, 0 },
    { "250-c1.c1",    0x00800000, 0x09a52c6f, ROMType::SPRITE, 0 },
    { "250-c2.c2",    0x00800000, 0x31679821, ROMType::SPRITE, 0 },
    { "250-c3.c3",    0x00800000, 0xfd602019, ROMType::SPRITE, 0 },
    { "250-c4.c4",    0x00800000, 0x31354513, ROMType::SPRITE, 0 },
    { "250-c5.c5",    0x00800000, 0xa4b56124, ROMType::SPRITE, 0 },
    { "250-c6.c6",    0x00800000, 0x83e3e69d, ROMType::SPRITE, 0 },
    { "250-m1.m1",    0x00020000, 0xfd42a842, ROMType::SOUND_PROGRAM, 0 },
    { "250-v1.v1",    0x00400000, 0xc79ede73, ROMType::SOUND_SAMPLE, 0 },
    { "250-v2.v2",    0x00400000, 0xea9aabe1, ROMType::SOUND_SAMPLE, 0 },
    { "250-v3.v3",    0x00200000, 0x2ca65102, ROMType::SOUND_SAMPLE, 0 },
};

// Minasan no Okagesamadesu! Dai Sugoroku Taikai (MOM-001 ~ MOH-001)
static const ROMEntry minasan_roms[] = {
    { "027-p1.p1",    0x00080000, 0xc8381327, ROMType::PROGRAM, 0 },
    { "027-s1.s1",    0x00020000, 0xe5824baa, ROMType::TEXT, 0 },
    { "027-c1.c1",    0x00100000, 0xd0086f94, ROMType::SPRITE, 0 },
    { "027-c2.c2",    0x00100000, 0xda61f5a6, ROMType::SPRITE, 0 },
    { "027-c3.c3",    0x00100000, 0x08df1228, ROMType::SPRITE, 0 },
    { "027-c4.c4",    0x00100000, 0x54e87696, ROMType::SPRITE, 0 },
    { "027-m1.m1",    0x00020000, 0xadd5a226, ROMType::SOUND_PROGRAM, 0 },
    { "027-v11.v11",  0x00100000, 0x59ad4459, ROMType::SOUND_SAMPLE, 0 },
    { "027-v21.v21",  0x00100000, 0xdf5b4eeb, ROMType::SOUND_SAMPLE, 0 },
};

// Money Puzzle Exchanger / Money Idol Exchanger
static const ROMEntry miexchng_roms[] = {
    { "231-p1.p1",    0x00080000, 0x61be1810, ROMType::PROGRAM, 0 },
    { "231-s1.s1",    0x00020000, 0xfe0c0c53, ROMType::TEXT, 0 },
    { "231-c1.c1",    0x00200000, 0x6c403ba3, ROMType::SPRITE, 0 },
    { "231-c2.c2",    0x00200000, 0x554bcd9b, ROMType::SPRITE, 0 },
    { "231-c3.c3",    0x00100000, 0x4f6f7a63, ROMType::SPRITE, 0 },
    { "231-c4.c4",    0x00100000, 0x2e35e71b, ROMType::SPRITE, 0 },
    { "231-m1.m1",    0x00020000, 0xde41301b, ROMType::SOUND_PROGRAM, 0 },
    { "231-v1.v1",    0x00400000, 0x113fb898, ROMType::SOUND_SAMPLE, 0 },
};

// Mutation Nation (NGM-014 ~ NGH-014)
static const ROMEntry mutnat_roms[] = {
    { "014-p1.p1",    0x00080000, 0x6f1699c8, ROMType::PROGRAM, 0 },
    { "014-s1.s1",    0x00020000, 0x99419733, ROMType::TEXT, 0 },
    { "014-c1.c1",    0x00100000, 0x5e4381bf, ROMType::SPRITE, 0 },
    { "014-c2.c2",    0x00100000, 0x69ba4e18, ROMType::SPRITE, 0 },
    { "014-c3.c3",    0x00100000, 0x890327d5, ROMType::SPRITE, 0 },
    { "014-c4.c4",    0x00100000, 0xe4002651, ROMType::SPRITE, 0 },
    { "014-m1.m1",    0x00020000, 0xb6683092, ROMType::SOUND_PROGRAM, 0 },
    { "014-v1.v1",    0x00100000, 0x25419296, ROMType::SOUND_SAMPLE, 0 },
    { "014-v2.v2",    0x00100000, 0x0de53d5e, ROMType::SOUND_SAMPLE, 0 },
};

// NAM-1975 (NGM-001 ~ NGH-001)
static const ROMEntry nam1975_roms[] = {
    { "001-p1.p1",    0x00080000, 0xcc9fc951, ROMType::PROGRAM, 0 },
    { "001-s1.s1",    0x00020000, 0x7988ba51, ROMType::TEXT, 0 },
    { "001-c1.c1",    0x00080000, 0x32ea98e1, ROMType::SPRITE, 0 },
    { "001-c2.c2",    0x00080000, 0xcbc4064c, ROMType::SPRITE, 0 },
    { "001-c3.c3",    0x00080000, 0x0151054c, ROMType::SPRITE, 0 },
    { "001-c4.c4",    0x00080000, 0x0a32570d, ROMType::SPRITE, 0 },
    { "001-c5.c5",    0x00080000, 0x90b74cc2, ROMType::SPRITE, 0 },
    { "001-c6.c6",    0x00080000, 0xe62bed58, ROMType::SPRITE, 0 },
    { "001-m1.m1",    0x00040000, 0xba874463, ROMType::SOUND_PROGRAM, 0 },
    { "001-v11.v11",  0x00080000, 0xa7c3d5e5, ROMType::SOUND_SAMPLE, 0 },
    { "001-v21.v21",  0x00080000, 0x55e670b3, ROMType::SOUND_SAMPLE, 0 },
    { "001-v22.v22",  0x00080000, 0xab0d8368, ROMType::SOUND_SAMPLE, 0 },
    { "001-v23.v23",  0x00080000, 0xdf468e28, ROMType::SOUND_SAMPLE, 0 },
};

// Neo Bomberman
static const ROMEntry neobombe_roms[] = {
    { "093-p1.p1",    0x00100000, 0xa1a71d0d, ROMType::PROGRAM, 0 },
    { "093-s1.s1",    0x00020000, 0x4b3fa119, ROMType::TEXT, 0 },
    { "093-c1.c1",    0x00400000, 0xd1f328f8, ROMType::SPRITE, 0 },
    { "093-c2.c2",    0x00400000, 0x82c49540, ROMType::SPRITE, 0 },
    { "093-c3.c3",    0x00080000, 0xe37578c5, ROMType::SPRITE, 0 },
    { "093-c4.c4",    0x00080000, 0x59826783, ROMType::SPRITE, 0 },
    { "093-m1.m1",    0x00020000, 0xe81e780b, ROMType::SOUND_PROGRAM, 0 },
    { "093-v1.v1",    0x00400000, 0x02abd4b0, ROMType::SOUND_SAMPLE, 0 },
    { "093-v2.v2",    0x00200000, 0xa92b8b3d, ROMType::SOUND_SAMPLE, 0 },
};

// Neo Drift Out - New Technology
static const ROMEntry neodrift_roms[] = {
    { "213-p1.p1",    0x00200000, 0xe397d798, ROMType::PROGRAM, 0 },
    { "213-s1.s1",    0x00020000, 0xb76b61bc, ROMType::TEXT, 0 },
    { "213-c1.c1",    0x00400000, 0x3edc8bd3, ROMType::SPRITE, 0 },
    { "213-c2.c2",    0x00400000, 0x46ae5f16, ROMType::SPRITE, 0 },
    { "213-m1.m1",    0x00020000, 0x200045f1, ROMType::SOUND_PROGRAM, 0 },
    { "213-v1.v1",    0x00200000, 0xa421c076, ROMType::SOUND_SAMPLE, 0 },
    { "213-v2.v2",    0x00200000, 0x233c7dd9, ROMType::SOUND_SAMPLE, 0 },
};

// Neo Mr. Do!
static const ROMEntry neomrdo_roms[] = {
    { "207-p1.p1",    0x00100000, 0x334ea51e, ROMType::PROGRAM, 0 },
    { "207-s1.s1",    0x00020000, 0x6aebafce, ROMType::TEXT, 0 },
    { "207-c1.c1",    0x00200000, 0xc7541b9d, ROMType::SPRITE, 0 },
    { "207-c2.c2",    0x00200000, 0xf57166d2, ROMType::SPRITE, 0 },
    { "207-m1.m1",    0x00020000, 0xb5b74a95, ROMType::SOUND_PROGRAM, 0 },
    { "207-v1.v1",    0x00200000, 0x4143c052, ROMType::SOUND_SAMPLE, 0 },
};

// Neo Turf Masters / Big Tournament Golf
static const ROMEntry turfmast_roms[] = {
    { "200-p1.p1",    0x00200000, 0x28c83048, ROMType::PROGRAM, 0 },
    { "200-s1.s1",    0x00020000, 0x9a5402b2, ROMType::TEXT, 0 },
    { "200-c1.c1",    0x00400000, 0x8e7bf41a, ROMType::SPRITE, 0 },
    { "200-c2.c2",    0x00400000, 0x5a65a8ce, ROMType::SPRITE, 0 },
    { "200-m1.m1",    0x00020000, 0x9994ac00, ROMType::SOUND_PROGRAM, 0 },
    { "200-v1.v1",    0x00200000, 0x00fd48d2, ROMType::SOUND_SAMPLE, 0 },
    { "200-v2.v2",    0x00200000, 0x082acb31, ROMType::SOUND_SAMPLE, 0 },
    { "200-v3.v3",    0x00200000, 0x7abca053, ROMType::SOUND_SAMPLE, 0 },
    { "200-v4.v4",    0x00200000, 0x6c7b4902, ROMType::SOUND_SAMPLE, 0 },
};

// Neo-Geo Cup '98 - The Road to the Victory
static const ROMEntry neocup98_roms[] = {
    { "244-p1.p1",    0x00200000, 0xf8fdb7a5, ROMType::PROGRAM, 0 },
    { "244-s1.s1",    0x00020000, 0x9bddb697, ROMType::TEXT, 0 },
    { "244-c1.c1",    0x00800000, 0xc7a62b23, ROMType::SPRITE, 0 },
    { "244-c2.c2",    0x00800000, 0x33aa0f35, ROMType::SPRITE, 0 },
    { "244-m1.m1",    0x00020000, 0xa701b276, ROMType::SOUND_PROGRAM, 0 },
    { "244-v1.v1",    0x00400000, 0x79def46d, ROMType::SOUND_SAMPLE, 0 },
    { "244-v2.v2",    0x00200000, 0xb231902f, ROMType::SOUND_SAMPLE, 0 },
};

// Nightmare in the Dark
static const ROMEntry nitd_roms[] = {
    { "260-p1.p1",    0x00080000, 0x61361082, ROMType::PROGRAM, 0 },
    { "260-c1.c1",    0x00800000, 0x147b0c7f, ROMType::SPRITE, 0 },
    { "260-c2.c2",    0x00800000, 0xd2b04b0d, ROMType::SPRITE, 0 },
    { "260-m1.m1",    0x00080000, 0x6407c5e5, ROMType::SOUND_PROGRAM, 0 },
    { "260-v1.v1",    0x00400000, 0x24b0480c, ROMType::SOUND_SAMPLE, 0 },
};

// Ninja Combat (NGM-009)
static const ROMEntry ncombat_roms[] = {
    { "009-p1.p1",    0x00080000, 0xb45fcfbf, ROMType::PROGRAM, 0 },
    { "009-s1.s1",    0x00020000, 0xd49afee8, ROMType::TEXT, 0 },
    { "009-c1.c1",    0x00080000, 0x33cc838e, ROMType::SPRITE, 0 },
    { "009-c2.c2",    0x00080000, 0x26877feb, ROMType::SPRITE, 0 },
    { "009-c3.c3",    0x00080000, 0x3b60a05d, ROMType::SPRITE, 0 },
    { "009-c4.c4",    0x00080000, 0x39c2d039, ROMType::SPRITE, 0 },
    { "009-c5.c5",    0x00080000, 0x67a4344e, ROMType::SPRITE, 0 },
    { "009-c6.c6",    0x00080000, 0x2eca8b19, ROMType::SPRITE, 0 },
    { "009-m1.m1",    0x00020000, 0xb5819863, ROMType::SOUND_PROGRAM, 0 },
    { "009-v11.v11",  0x00080000, 0xcf32a59c, ROMType::SOUND_SAMPLE, 0 },
    { "009-v12.v12",  0x00080000, 0x7b3588b7, ROMType::SOUND_SAMPLE, 0 },
    { "009-v13.v13",  0x00080000, 0x505a01b5, ROMType::SOUND_SAMPLE, 0 },
    { "009-v21.v21",  0x00080000, 0x365f9011, ROMType::SOUND_SAMPLE, 0 },
};

// Ninja Commando
static const ROMEntry ncommand_roms[] = {
    { "050-p1.p1",    0x00100000, 0x4e097c40, ROMType::PROGRAM, 0 },
    { "050-s1.s1",    0x00020000, 0xdb8f9c8e, ROMType::TEXT, 0 },
    { "050-c1.c1",    0x00100000, 0x87421a0a, ROMType::SPRITE, 0 },
    { "050-c2.c2",    0x00100000, 0xc4cf5548, ROMType::SPRITE, 0 },
    { "050-c3.c3",    0x00100000, 0x03422c1e, ROMType::SPRITE, 0 },
    { "050-c4.c4",    0x00100000, 0x0845eadb, ROMType::SPRITE, 0 },
    { "050-m1.m1",    0x00020000, 0x6fcf07d3, ROMType::SOUND_PROGRAM, 0 },
    { "050-v1.v1",    0x00100000, 0x23c3ab42, ROMType::SOUND_SAMPLE, 0 },
    { "050-v2.v2",    0x00080000, 0x80b8a984, ROMType::SOUND_SAMPLE, 0 },
};

// Ninja Master's - Haoh-ninpo-cho
static const ROMEntry ninjamas_roms[] = {
    { "217-p1.p1",    0x00100000, 0x3e97ed69, ROMType::PROGRAM, 0 },
    { "217-p2.sp2",   0x00200000, 0x191fca88, ROMType::PROGRAM, 0 },
    { "217-s1.s1",    0x00020000, 0x8ff782f0, ROMType::TEXT, 0 },
    { "217-c1.c1",    0x00400000, 0x5fe97bc4, ROMType::SPRITE, 0 },
    { "217-c2.c2",    0x00400000, 0x886e0d66, ROMType::SPRITE, 0 },
    { "217-c3.c3",    0x00400000, 0x59e8525f, ROMType::SPRITE, 0 },
    { "217-c4.c4",    0x00400000, 0x8521add2, ROMType::SPRITE, 0 },
    { "217-c5.c5",    0x00400000, 0xfb1896e5, ROMType::SPRITE, 0 },
    { "217-c6.c6",    0x00400000, 0x1c98c54b, ROMType::SPRITE, 0 },
    { "217-c7.c7",    0x00400000, 0x8b0ede2e, ROMType::SPRITE, 0 },
    { "217-c8.c8",    0x00400000, 0xa085bb61, ROMType::SPRITE, 0 },
    { "217-m1.m1",    0x00020000, 0xd00fb2af, ROMType::SOUND_PROGRAM, 0 },
    { "217-v1.v1",    0x00400000, 0x1c34e013, ROMType::SOUND_SAMPLE, 0 },
    { "217-v2.v2",    0x00200000, 0x22f1c681, ROMType::SOUND_SAMPLE, 0 },
};

// Over Top
static const ROMEntry overtop_roms[] = {
    { "212-p1.p1",    0x00200000, 0x16c063a9, ROMType::PROGRAM, 0 },
    { "212-s1.s1",    0x00020000, 0x481d3ddc, ROMType::TEXT, 0 },
    { "212-c1.c1",    0x00400000, 0x50f43087, ROMType::SPRITE, 0 },
    { "212-c2.c2",    0x00400000, 0xa5b39807, ROMType::SPRITE, 0 },
    { "212-c3.c3",    0x00400000, 0x9252ea02, ROMType::SPRITE, 0 },
    { "212-c4.c4",    0x00400000, 0x5f41a699, ROMType::SPRITE, 0 },
    { "212-c5.c5",    0x00200000, 0xfc858bef, ROMType::SPRITE, 0 },
    { "212-c6.c6",    0x00200000, 0x0589c15e, ROMType::SPRITE, 0 },
    { "212-m1.m1",    0x00020000, 0xfcab6191, ROMType::SOUND_PROGRAM, 0 },
    { "212-v1.v1",    0x00400000, 0x013d4ef9, ROMType::SOUND_SAMPLE, 0 },
};

// Panic Bomber
static const ROMEntry panicbom_roms[] = {
    { "073-p1.p1",    0x00080000, 0xadc356ad, ROMType::PROGRAM, 0 },
    { "073-s1.s1",    0x00020000, 0xb876de7e, ROMType::TEXT, 0 },
    { "073-c1.c1",    0x00100000, 0x8582e1b5, ROMType::SPRITE, 0 },
    { "073-c2.c2",    0x00100000, 0xe15a093b, ROMType::SPRITE, 0 },
    { "073-m1.m1",    0x00020000, 0x3cdf5d88, ROMType::SOUND_PROGRAM, 0 },
    { "073-v1.v1",    0x00200000, 0x7fc86d2f, ROMType::SOUND_SAMPLE, 0 },
    { "073-v2.v2",    0x00100000, 0x082adfc7, ROMType::SOUND_SAMPLE, 0 },
};

// Pleasure Goal / Futsal - 5 on 5 Mini Soccer (NGM-219)
static const ROMEntry pgoal_roms[] = {
    { "219-p1.p1",    0x00200000, 0x6af0e574, ROMType::PROGRAM, 0 },
    { "219-s1.s1",    0x00020000, 0x002f3c88, ROMType::TEXT, 0 },
    { "219-c1.c1",    0x00400000, 0x67fec4dc, ROMType::SPRITE, 0 },
    { "219-c2.c2",    0x00400000, 0x86ed01f2, ROMType::SPRITE, 0 },
    { "219-c3.c3",    0x00200000, 0x5fdad0a5, ROMType::SPRITE, 0 },
    { "219-c4.c4",    0x00200000, 0xf57b4a1c, ROMType::SPRITE, 0 },
    { "219-m1.m1",    0x00020000, 0x958efdc8, ROMType::SOUND_PROGRAM, 0 },
    { "219-v1.v1",    0x00400000, 0xd0ae33d9, ROMType::SOUND_SAMPLE, 0 },
};

// Pochi and Nyaa (Ver 2.02)
static const ROMEntry pnyaa_roms[] = {
    { "pn202.p1",     0x00100000, 0xbf34e71c, ROMType::PROGRAM, 0 },
    { "267-c1.c1",    0x00800000, 0x5eebee65, ROMType::SPRITE, 0 },
    { "267-c2.c2",    0x00800000, 0x2b67187b, ROMType::SPRITE, 0 },
    { "m1.m1",        0x00080000, 0xc7853ccd, ROMType::SOUND_PROGRAM, 0 },
    { "267-v1.v1",    0x00400000, 0xe2e8e917, ROMType::SOUND_SAMPLE, 0 },
};

// Pop 'n Bounce / Gapporin
static const ROMEntry popbounc_roms[] = {
    { "237-p1.p1",    0x00100000, 0xbe96e44f, ROMType::PROGRAM, 0 },
    { "237-s1.s1",    0x00020000, 0xb61cf595, ROMType::TEXT, 0 },
    { "237-c1.c1",    0x00200000, 0xeda42d66, ROMType::SPRITE, 0 },
    { "237-c2.c2",    0x00200000, 0x5e633c65, ROMType::SPRITE, 0 },
    { "237-m1.m1",    0x00020000, 0xd4c946dd, ROMType::SOUND_PROGRAM, 0 },
    { "237-v1.v1",    0x00200000, 0xedcb1beb, ROMType::SOUND_SAMPLE, 0 },
};

// Power Spikes II (NGM-068)
static const ROMEntry pspikes2_roms[] = {
    { "068-pg1.p1",   0x00100000, 0x105a408f, ROMType::PROGRAM, 0 },
    { "068-sg1.s1",   0x00020000, 0x18082299, ROMType::TEXT, 0 },
    { "068-c1.c1",    0x00100000, 0x7f250f76, ROMType::SPRITE, 0 },
    { "068-c2.c2",    0x00100000, 0x20912873, ROMType::SPRITE, 0 },
    { "068-c3.c3",    0x00100000, 0x4b641ba1, ROMType::SPRITE, 0 },
    { "068-c4.c4",    0x00100000, 0x35072596, ROMType::SPRITE, 0 },
    { "068-c5.c5",    0x00100000, 0x151dd624, ROMType::SPRITE, 0 },
    { "068-c6.c6",    0x00100000, 0xa6722604, ROMType::SPRITE, 0 },
    { "068-mg1.m1",   0x00020000, 0xb1c7911e, ROMType::SOUND_PROGRAM, 0 },
    { "068-v1.v1",    0x00100000, 0x2ced86df, ROMType::SOUND_SAMPLE, 0 },
    { "068-v2.v2",    0x00100000, 0x970851ab, ROMType::SOUND_SAMPLE, 0 },
    { "068-v3.v3",    0x00100000, 0x81ff05aa, ROMType::SOUND_SAMPLE, 0 },
};

// Prehistoric Isle 2
static const ROMEntry preisle2_roms[] = {
    { "255-p1.p1",    0x00100000, 0xdfa3c0f3, ROMType::PROGRAM, 0 },
    { "255-p2.sp2",   0x00400000, 0x42050b80, ROMType::PROGRAM, 0 },
    { "255-c1.c1",    0x00800000, 0xea06000b, ROMType::SPRITE, 0 },
    { "255-c2.c2",    0x00800000, 0x04e67d79, ROMType::SPRITE, 0 },
    { "255-c3.c3",    0x00800000, 0x60e31e08, ROMType::SPRITE, 0 },
    { "255-c4.c4",    0x00800000, 0x40371d69, ROMType::SPRITE, 0 },
    { "255-c5.c5",    0x00800000, 0x0b2e6adf, ROMType::SPRITE, 0 },
    { "255-c6.c6",    0x00800000, 0xb001bdd3, ROMType::SPRITE, 0 },
    { "255-m1.m1",    0x00020000, 0x8efd4014, ROMType::SOUND_PROGRAM, 0 },
    { "255-v1.v1",    0x00400000, 0x5a14543d, ROMType::SOUND_SAMPLE, 0 },
    { "255-v2.v2",    0x00200000, 0x6610d91a, ROMType::SOUND_SAMPLE, 0 },
};

// Pulstar
static const ROMEntry pulstar_roms[] = {
    { "089-p1.p1",    0x00100000, 0x5e5847a2, ROMType::PROGRAM, 0 },
    { "089-p2.sp2",   0x00200000, 0x028b774c, ROMType::PROGRAM, 0 },
    { "089-s1.s1",    0x00020000, 0xc79fc2c8, ROMType::TEXT, 0 },
    { "089-c1.c1",    0x00400000, 0xf4e97332, ROMType::SPRITE, 0 },
    { "089-c2.c2",    0x00400000, 0x836d14da, ROMType::SPRITE, 0 },
    { "089-c3.c3",    0x00400000, 0x913611c4, ROMType::SPRITE, 0 },
    { "089-c4.c4",    0x00400000, 0x44cef0e3, ROMType::SPRITE, 0 },
    { "089-c5.c5",    0x00400000, 0x89baa1d7, ROMType::SPRITE, 0 },
    { "089-c6.c6",    0x00400000, 0xb2594d56, ROMType::SPRITE, 0 },
    { "089-c7.c7",    0x00200000, 0x6a5618ca, ROMType::SPRITE, 0 },
    { "089-c8.c8",    0x00200000, 0xa223572d, ROMType::SPRITE, 0 },
    { "089-m1.m1",    0x00020000, 0xff3df7c7, ROMType::SOUND_PROGRAM, 0 },
    { "089-v1.v1",    0x00400000, 0x6f726ecb, ROMType::SOUND_SAMPLE, 0 },
    { "089-v2.v2",    0x00400000, 0x9d2db551, ROMType::SOUND_SAMPLE, 0 },
};

// Puzzle Bobble / Bust-A-Move (Neo-Geo, NGM-083)
static const ROMEntry pbobblen_roms[] = {
    { "d96-07.ep1",   0x00080000, 0x6102ca14, ROMType::PROGRAM, 0 },
    { "d96-04.s1",    0x00020000, 0x9caae538, ROMType::TEXT, 0 },
    { "068-c1.c1",    0x00100000, 0x7f250f76, ROMType::SPRITE, 0 },
    { "068-c2.c2",    0x00100000, 0x20912873, ROMType::SPRITE, 0 },
    { "068-c3.c3",    0x00100000, 0x4b641ba1, ROMType::SPRITE, 0 },
    { "068-c4.c4",    0x00100000, 0x35072596, ROMType::SPRITE, 0 },
    { "d96-02.c5",    0x00080000, 0xe89ad494, ROMType::SPRITE, 0 },
    { "d96-03.c6",    0x00080000, 0x4b42d7eb, ROMType::SPRITE, 0 },
    { "d96-06.m1",    0x00020000, 0xf424368a, ROMType::SOUND_PROGRAM, 0 },
    { "068-v1.v1",    0x00100000, 0x2ced86df, ROMType::SOUND_SAMPLE, 0 },
    { "068-v2.v2",    0x00100000, 0x970851ab, ROMType::SOUND_SAMPLE, 0 },
    { "d96-01.v3",    0x00100000, 0x0840cbc4, ROMType::SOUND_SAMPLE, 0 },
    { "d96-05.v4",    0x00080000, 0x0a548948, ROMType::SOUND_SAMPLE, 0 },
};

// Puzzle Bobble 2 / Bust-A-Move Again (Neo-Geo)
static const ROMEntry pbobbl2n_roms[] = {
    { "248-p1.p1",    0x00100000, 0x9d6c0754, ROMType::PROGRAM, 0 },
    { "248-s1.s1",    0x00020000, 0x0a3fee41, ROMType::TEXT, 0 },
    { "248-c1.c1",    0x00400000, 0xd9115327, ROMType::SPRITE, 0 },
    { "248-c2.c2",    0x00400000, 0x77f9fdac, ROMType::SPRITE, 0 },
    { "248-c3.c3",    0x00100000, 0x8890bf7c, ROMType::SPRITE, 0 },
    { "248-c4.c4",    0x00100000, 0x8efead3f, ROMType::SPRITE, 0 },
    { "248-m1.m1",    0x00020000, 0x883097a9, ROMType::SOUND_PROGRAM, 0 },
    { "248-v1.v1",    0x00400000, 0x57fde1fa, ROMType::SOUND_SAMPLE, 0 },
    { "248-v2.v2",    0x00400000, 0x4b966ef3, ROMType::SOUND_SAMPLE, 0 },
};

// Puzzle De Pon! R!
static const ROMEntry puzzldpr_roms[] = {
    { "235-p1.p1",    0x00080000, 0xafed5de2, ROMType::PROGRAM, 0 },
    { "235-s1.s1",    0x00020000, 0x3b13a22f, ROMType::TEXT, 0 },
    { "202-c1.c1",    0x00100000, 0xcc0095ef, ROMType::SPRITE, 0 },
    { "202-c2.c2",    0x00100000, 0x42371307, ROMType::SPRITE, 0 },
    { "202-m1.m1",    0x00020000, 0x9c0291ea, ROMType::SOUND_PROGRAM, 0 },
    { "202-v1.v1",    0x00080000, 0xdebeb8fb, ROMType::SOUND_SAMPLE, 0 },
};

// Puzzle De Pon!
static const ROMEntry puzzledp_roms[] = {
    { "202-p1.p1",    0x00080000, 0x2b61415b, ROMType::PROGRAM, 0 },
    { "202-s1.s1",    0x00020000, 0xcd19264f, ROMType::TEXT, 0 },
    { "202-c1.c1",    0x00100000, 0xcc0095ef, ROMType::SPRITE, 0 },
    { "202-c2.c2",    0x00100000, 0x42371307, ROMType::SPRITE, 0 },
    { "202-m1.m1",    0x00020000, 0x9c0291ea, ROMType::SOUND_PROGRAM, 0 },
    { "202-v1.v1",    0x00080000, 0xdebeb8fb, ROMType::SOUND_SAMPLE, 0 },
};

// Puzzled / Joy Joy Kid (NGM-021 ~ NGH-021)
static const ROMEntry joyjoy_roms[] = {
    { "021-p1.p1",    0x00080000, 0x39c3478f, ROMType::PROGRAM, 0 },
    { "021-s1.s1",    0x00020000, 0x6956d778, ROMType::TEXT, 0 },
    { "021-c1.c1",    0x00080000, 0x509250ec, ROMType::SPRITE, 0 },
    { "021-c2.c2",    0x00080000, 0x09ed5258, ROMType::SPRITE, 0 },
    { "021-m1.m1",    0x00040000, 0x5a4be5e8, ROMType::SOUND_PROGRAM, 0 },
    { "021-v11.v11",  0x00080000, 0x66c1e5c4, ROMType::SOUND_SAMPLE, 0 },
    { "021-v21.v21",  0x00080000, 0x8ed20a86, ROMType::SOUND_SAMPLE, 0 },
};

// Quiz Daisousa Sen - The Last Count Down (NGM-023 ~ NGH-023)
static const ROMEntry quizdais_roms[] = {
    { "023-p1.p1",    0x00100000, 0xc488fda3, ROMType::PROGRAM, 0 },
    { "023-s1.s1",    0x00020000, 0xac31818a, ROMType::TEXT, 0 },
    { "023-c1.c1",    0x00100000, 0x2999535a, ROMType::SPRITE, 0 },
    { "023-c2.c2",    0x00100000, 0x876a99e6, ROMType::SPRITE, 0 },
    { "023-m1.m1",    0x00020000, 0x2a2105e0, ROMType::SOUND_PROGRAM, 0 },
    { "023-v1.v1",    0x00100000, 0xa53e5bd3, ROMType::SOUND_SAMPLE, 0 },
};

// Quiz King of Fighters (SAM-080 ~ SAH-080)
static const ROMEntry quizkof_roms[] = {
    { "080-p1.p1",    0x00100000, 0x4440315e, ROMType::PROGRAM, 0 },
    { "080-s1.s1",    0x00020000, 0xd7b86102, ROMType::TEXT, 0 },
    { "080-c1.c1",    0x00200000, 0xea1d764a, ROMType::SPRITE, 0 },
    { "080-c2.c2",    0x00200000, 0xd331d4a4, ROMType::SPRITE, 0 },
    { "080-c3.c3",    0x00200000, 0xb4851bfe, ROMType::SPRITE, 0 },
    { "080-c4.c4",    0x00200000, 0xca6f5460, ROMType::SPRITE, 0 },
    { "080-m1.m1",    0x00020000, 0xf5f44172, ROMType::SOUND_PROGRAM, 0 },
    { "080-v1.v1",    0x00200000, 0x0be18f60, ROMType::SOUND_SAMPLE, 0 },
    { "080-v2.v2",    0x00200000, 0x4abde3ff, ROMType::SOUND_SAMPLE, 0 },
    { "080-v3.v3",    0x00200000, 0xf02844e2, ROMType::SOUND_SAMPLE, 0 },
};

// Quiz Meitantei Neo & Geo - Quiz Daisousa Sen part 2 (NGM-042 ~ NGH-042)
static const ROMEntry quizdai2_roms[] = {
    { "042-p1.p1",    0x00100000, 0xed719dcf, ROMType::PROGRAM, 0 },
    { "042-s1.s1",    0x00020000, 0x164fd6e6, ROMType::TEXT, 0 },
    { "042-c1.c1",    0x00100000, 0xcb5809a1, ROMType::SPRITE, 0 },
    { "042-c2.c2",    0x00100000, 0x1436dfeb, ROMType::SPRITE, 0 },
    { "042-c3.c3",    0x00080000, 0xbcd4a518, ROMType::SPRITE, 0 },
    { "042-c4.c4",    0x00080000, 0xd602219b, ROMType::SPRITE, 0 },
    { "042-m1.m1",    0x00020000, 0xbb19995d, ROMType::SOUND_PROGRAM, 0 },
    { "042-v1.v1",    0x00100000, 0xaf7f8247, ROMType::SOUND_SAMPLE, 0 },
    { "042-v2.v2",    0x00100000, 0xc6474b59, ROMType::SOUND_SAMPLE, 0 },
};

// Rage of the Dragons (NGM-2640?)
static const ROMEntry rotd_roms[] = {
    { "264-p1.p1",    0x00800000, 0xb8cc969d, ROMType::PROGRAM, 0 },
    { "264-c1.c1",    0x00800000, 0x4f148fee, ROMType::SPRITE, 0 },
    { "264-c2.c2",    0x00800000, 0x7cf5ff72, ROMType::SPRITE, 0 },
    { "264-c3.c3",    0x00800000, 0x64d84c98, ROMType::SPRITE, 0 },
    { "264-c4.c4",    0x00800000, 0x2f394a95, ROMType::SPRITE, 0 },
    { "264-c5.c5",    0x00800000, 0x6b99b978, ROMType::SPRITE, 0 },
    { "264-c6.c6",    0x00800000, 0x847d5c7d, ROMType::SPRITE, 0 },
    { "264-c7.c7",    0x00800000, 0x231d681e, ROMType::SPRITE, 0 },
    { "264-c8.c8",    0x00800000, 0xc5edb5c4, ROMType::SPRITE, 0 },
    { "264-m1.m1",    0x00020000, 0x4dbd7b43, ROMType::SOUND_PROGRAM, 0 },
    { "264-v1.v1",    0x00800000, 0xfa005812, ROMType::SOUND_SAMPLE, 0 },
    { "264-v2.v2",    0x00800000, 0xc3dc8bf0, ROMType::SOUND_SAMPLE, 0 },
};

// Ragnagard / Shin-Oh-Ken
static const ROMEntry ragnagrd_roms[] = {
    { "218-p1.p1",    0x00200000, 0xca372303, ROMType::PROGRAM, 0 },
    { "218-s1.s1",    0x00020000, 0x7d402f9a, ROMType::TEXT, 0 },
    { "218-c1.c1",    0x00400000, 0xc31500a4, ROMType::SPRITE, 0 },
    { "218-c2.c2",    0x00400000, 0x98aba1f9, ROMType::SPRITE, 0 },
    { "218-c3.c3",    0x00400000, 0x833c163a, ROMType::SPRITE, 0 },
    { "218-c4.c4",    0x00400000, 0xc1a30f69, ROMType::SPRITE, 0 },
    { "218-c5.c5",    0x00400000, 0x6b6de0ff, ROMType::SPRITE, 0 },
    { "218-c6.c6",    0x00400000, 0x94beefcf, ROMType::SPRITE, 0 },
    { "218-c7.c7",    0x00400000, 0xde6f9b28, ROMType::SPRITE, 0 },
    { "218-c8.c8",    0x00400000, 0xd9b311f6, ROMType::SPRITE, 0 },
    { "218-m1.m1",    0x00020000, 0x17028bcf, ROMType::SOUND_PROGRAM, 0 },
    { "218-v1.v1",    0x00400000, 0x61eee7f4, ROMType::SOUND_SAMPLE, 0 },
    { "218-v2.v2",    0x00400000, 0x6104e20b, ROMType::SOUND_SAMPLE, 0 },
};

// Real Bout Fatal Fury / Real Bout Garou Densetsu (NGM-095 ~ NGH-095)
static const ROMEntry rbff1_roms[] = {
    { "095-p1.p1",    0x00100000, 0x63b4d8ae, ROMType::PROGRAM, 0 },
    { "095-p2.sp2",   0x00200000, 0xcc15826e, ROMType::PROGRAM, 0 },
    { "095-s1.s1",    0x00020000, 0xb6bf5e08, ROMType::TEXT, 0 },
    { "069-c1.c1",    0x00400000, 0xe302f93c, ROMType::SPRITE, 0 },
    { "069-c2.c2",    0x00400000, 0x1053a455, ROMType::SPRITE, 0 },
    { "069-c3.c3",    0x00400000, 0x1c0fde2f, ROMType::SPRITE, 0 },
    { "069-c4.c4",    0x00400000, 0xa25fc3d0, ROMType::SPRITE, 0 },
    { "095-c5.c5",    0x00400000, 0x8b9b65df, ROMType::SPRITE, 0 },
    { "095-c6.c6",    0x00400000, 0x3e164718, ROMType::SPRITE, 0 },
    { "095-c7.c7",    0x00200000, 0xca605e12, ROMType::SPRITE, 0 },
    { "095-c8.c8",    0x00200000, 0x4e6beb6c, ROMType::SPRITE, 0 },
    { "095-m1.m1",    0x00020000, 0x653492a7, ROMType::SOUND_PROGRAM, 0 },
    { "069-v1.v1",    0x00400000, 0x2bdbd4db, ROMType::SOUND_SAMPLE, 0 },
    { "069-v2.v2",    0x00400000, 0xa698a487, ROMType::SOUND_SAMPLE, 0 },
    { "095-v3.v3",    0x00400000, 0x189d1c6c, ROMType::SOUND_SAMPLE, 0 },
};

// Real Bout Fatal Fury 2 - The Newcomers / Real Bout Garou Densetsu 2 - The Newcomers (NGM-2400)
static const ROMEntry rbff2_roms[] = {
    { "240-p1.p1",    0x00100000, 0x80e41205, ROMType::PROGRAM, 0 },
    { "240-p2.sp2",   0x00400000, 0x960aa88d, ROMType::PROGRAM, 0 },
    { "240-s1.s1",    0x00020000, 0xda3b40de, ROMType::TEXT, 0 },
    { "240-c1.c1",    0x00800000, 0xeffac504, ROMType::SPRITE, 0 },
    { "240-c2.c2",    0x00800000, 0xed182d44, ROMType::SPRITE, 0 },
    { "240-c3.c3",    0x00800000, 0x22e0330a, ROMType::SPRITE, 0 },
    { "240-c4.c4",    0x00800000, 0xc19a07eb, ROMType::SPRITE, 0 },
    { "240-c5.c5",    0x00800000, 0x244dff5a, ROMType::SPRITE, 0 },
    { "240-c6.c6",    0x00800000, 0x4609e507, ROMType::SPRITE, 0 },
    { "240-m1.m1",    0x00040000, 0xed482791, ROMType::SOUND_PROGRAM, 0 },
    { "240-v1.v1",    0x00400000, 0xf796265a, ROMType::SOUND_SAMPLE, 0 },
    { "240-v2.v2",    0x00400000, 0x2cb3f3bb, ROMType::SOUND_SAMPLE, 0 },
    { "240-v3.v3",    0x00400000, 0x8fe1367a, ROMType::SOUND_SAMPLE, 0 },
    { "240-v4.v4",    0x00200000, 0x996704d8, ROMType::SOUND_SAMPLE, 0 },
};

// Real Bout Fatal Fury Special / Real Bout Garou Densetsu Special
static const ROMEntry rbffspec_roms[] = {
    { "223-p1.p1",    0x00100000, 0xf84a2d1d, ROMType::PROGRAM, 0 },
    { "223-p2.sp2",   0x00400000, 0xaddd8f08, ROMType::PROGRAM, 0 },
    { "223-s1.s1",    0x00020000, 0x7ecd6e8c, ROMType::TEXT, 0 },
    { "223-c1.c1",    0x00400000, 0xebab05e2, ROMType::SPRITE, 0 },
    { "223-c2.c2",    0x00400000, 0x641868c3, ROMType::SPRITE, 0 },
    { "223-c3.c3",    0x00400000, 0xca00191f, ROMType::SPRITE, 0 },
    { "223-c4.c4",    0x00400000, 0x1f23d860, ROMType::SPRITE, 0 },
    { "223-c5.c5",    0x00400000, 0x321e362c, ROMType::SPRITE, 0 },
    { "223-c6.c6",    0x00400000, 0xd8fcef90, ROMType::SPRITE, 0 },
    { "223-c7.c7",    0x00400000, 0xbc80dd2d, ROMType::SPRITE, 0 },
    { "223-c8.c8",    0x00400000, 0x5ad62102, ROMType::SPRITE, 0 },
    { "223-m1.m1",    0x00020000, 0x3fee46bf, ROMType::SOUND_PROGRAM, 0 },
    { "223-v1.v1",    0x00400000, 0x76673869, ROMType::SOUND_SAMPLE, 0 },
    { "223-v2.v2",    0x00400000, 0x7a275acd, ROMType::SOUND_SAMPLE, 0 },
    { "223-v3.v3",    0x00400000, 0x5a797fd2, ROMType::SOUND_SAMPLE, 0 },
};

// Riding Hero (NGM-006 ~ NGH-006)
static const ROMEntry ridhero_roms[] = {
    { "006-p1.p1",    0x00080000, 0xd4aaf597, ROMType::PROGRAM, 0 },
    { "006-s1.s1",    0x00020000, 0xeb5189f0, ROMType::TEXT, 0 },
    { "006-c1.c1",    0x00080000, 0x4a5c7f78, ROMType::SPRITE, 0 },
    { "006-c2.c2",    0x00080000, 0xe0b70ece, ROMType::SPRITE, 0 },
    { "006-c3.c3",    0x00080000, 0x8acff765, ROMType::SPRITE, 0 },
    { "006-c4.c4",    0x00080000, 0x205e3208, ROMType::SPRITE, 0 },
    { "006-m1.m1",    0x00040000, 0x92e7b4fe, ROMType::SOUND_PROGRAM, 0 },
    { "006-v11.v11",  0x00080000, 0xcdf74a42, ROMType::SOUND_SAMPLE, 0 },
    { "006-v12.v12",  0x00080000, 0xe2fd2371, ROMType::SOUND_SAMPLE, 0 },
    { "006-v21.v21",  0x00080000, 0x94092bce, ROMType::SOUND_SAMPLE, 0 },
    { "006-v22.v22",  0x00080000, 0x4e2cd7c3, ROMType::SOUND_SAMPLE, 0 },
    { "006-v23.v23",  0x00080000, 0x069c71ed, ROMType::SOUND_SAMPLE, 0 },
    { "006-v24.v24",  0x00080000, 0x89fbb825, ROMType::SOUND_SAMPLE, 0 },
};

// Robo Army
static const ROMEntry roboarmy_roms[] = {
    { "032-p1.p1",    0x00080000, 0xcd11cbd4, ROMType::PROGRAM, 0 },
    { "032-s1.s1",    0x00020000, 0xac0daa1b, ROMType::TEXT, 0 },
    { "032-c1.c1",    0x00100000, 0x97984c6c, ROMType::SPRITE, 0 },
    { "032-c2.c2",    0x00100000, 0x65773122, ROMType::SPRITE, 0 },
    { "032-c3.c3",    0x00080000, 0x40adfccd, ROMType::SPRITE, 0 },
    { "032-c4.c4",    0x00080000, 0x462571de, ROMType::SPRITE, 0 },
    { "032-m1.m1",    0x00020000, 0x35ec952d, ROMType::SOUND_PROGRAM, 0 },
    { "032-v1.v1",    0x00100000, 0x63791533, ROMType::SOUND_SAMPLE, 0 },
    { "032-v2.v2",    0x00100000, 0xeb95de70, ROMType::SOUND_SAMPLE, 0 },
};

// Samurai Shodown / Samurai Spirits (NGM-045)
static const ROMEntry samsho_roms[] = {
    { "045-p1.p1",    0x00100000, 0xdfe51bf0, ROMType::PROGRAM, 0 },
    { "045-pg2.sp2",  0x00100000, 0x46745b94, ROMType::PROGRAM, 0 },
    { "045-s1.s1",    0x00020000, 0x9142a4d3, ROMType::TEXT, 0 },
    { "045-c1.c1",    0x00200000, 0x2e5873a4, ROMType::SPRITE, 0 },
    { "045-c2.c2",    0x00200000, 0x04febb10, ROMType::SPRITE, 0 },
    { "045-c3.c3",    0x00200000, 0xf3dabd1e, ROMType::SPRITE, 0 },
    { "045-c4.c4",    0x00200000, 0x935c62f0, ROMType::SPRITE, 0 },
    { "045-c51.c5",   0x00100000, 0x81932894, ROMType::SPRITE, 0 },
    { "045-c61.c6",   0x00100000, 0xbe30612e, ROMType::SPRITE, 0 },
    { "045-m1.m1",    0x00020000, 0x95170640, ROMType::SOUND_PROGRAM, 0 },
    { "045-v1.v1",    0x00200000, 0x37f78a9b, ROMType::SOUND_SAMPLE, 0 },
    { "045-v2.v2",    0x00200000, 0x568b20cf, ROMType::SOUND_SAMPLE, 0 },
};

// Samurai Shodown II / Shin Samurai Spirits - Haohmaru Jigokuhen (NGM-063 ~ NGH-063)
static const ROMEntry samsho2_roms[] = {
    { "063-p1.p1",    0x00200000, 0x22368892, ROMType::PROGRAM, 0 },
    { "063-s1.s1",    0x00020000, 0x64a5cd66, ROMType::TEXT, 0 },
    { "063-c1.c1",    0x00200000, 0x86cd307c, ROMType::SPRITE, 0 },
    { "063-c2.c2",    0x00200000, 0xcdfcc4ca, ROMType::SPRITE, 0 },
    { "063-c3.c3",    0x00200000, 0x7a63ccc7, ROMType::SPRITE, 0 },
    { "063-c4.c4",    0x00200000, 0x751025ce, ROMType::SPRITE, 0 },
    { "063-c5.c5",    0x00200000, 0x20d3a475, ROMType::SPRITE, 0 },
    { "063-c6.c6",    0x00200000, 0xae4c0a88, ROMType::SPRITE, 0 },
    { "063-c7.c7",    0x00200000, 0x2df3cbcf, ROMType::SPRITE, 0 },
    { "063-c8.c8",    0x00200000, 0x1ffc6dfa, ROMType::SPRITE, 0 },
    { "063-m1.m1",    0x00020000, 0x56675098, ROMType::SOUND_PROGRAM, 0 },
    { "063-v1.v1",    0x00200000, 0x37703f91, ROMType::SOUND_SAMPLE, 0 },
    { "063-v2.v2",    0x00200000, 0x0142bde8, ROMType::SOUND_SAMPLE, 0 },
    { "063-v3.v3",    0x00200000, 0xd07fa5ca, ROMType::SOUND_SAMPLE, 0 },
    { "063-v4.v4",    0x00100000, 0x24aab4bb, ROMType::SOUND_SAMPLE, 0 },
};

// Samurai Shodown III / Samurai Spirits - Zankurou Musouken (NGM-087)
static const ROMEntry samsho3_roms[] = {
    { "087-epr.ep1",  0x00080000, 0x23e09bb8, ROMType::PROGRAM, 0 },
    { "087-epr.ep2",  0x00080000, 0x256f5302, ROMType::PROGRAM, 0 },
    { "087-epr.ep3",  0x00080000, 0xbf2db5dd, ROMType::PROGRAM, 0 },
    { "087-epr.ep4",  0x00080000, 0x53e60c58, ROMType::PROGRAM, 0 },
    { "087-p5.p5",    0x00100000, 0xe86ca4af, ROMType::PROGRAM, 0 },
    { "087-s1.s1",    0x00020000, 0x74ec7d9f, ROMType::TEXT, 0 },
    { "087-c1.c1",    0x00400000, 0x07a233bc, ROMType::SPRITE, 0 },
    { "087-c2.c2",    0x00400000, 0x7a413592, ROMType::SPRITE, 0 },
    { "087-c3.c3",    0x00400000, 0x8b793796, ROMType::SPRITE, 0 },
    { "087-c4.c4",    0x00400000, 0x728fbf11, ROMType::SPRITE, 0 },
    { "087-c5.c5",    0x00400000, 0x172ab180, ROMType::SPRITE, 0 },
    { "087-c6.c6",    0x00400000, 0x002ff8f3, ROMType::SPRITE, 0 },
    { "087-c7.c7",    0x00100000, 0xae450e3d, ROMType::SPRITE, 0 },
    { "087-c8.c8",    0x00100000, 0xa9e82717, ROMType::SPRITE, 0 },
    { "087-m1.m1",    0x00020000, 0x8e6440eb, ROMType::SOUND_PROGRAM, 0 },
    { "087-v1.v1",    0x00400000, 0x84bdd9a0, ROMType::SOUND_SAMPLE, 0 },
    { "087-v2.v2",    0x00200000, 0xac0f261a, ROMType::SOUND_SAMPLE, 0 },
};

// Samurai Shodown IV - Amakusa's Revenge / Samurai Spirits - Amakusa Kourin (NGM-222 ~ NGH-222)
static const ROMEntry samsho4_roms[] = {
    { "222-p1.p1",    0x00100000, 0x1a5cb56d, ROMType::PROGRAM, 0 },
    { "222-p2.sp2",   0x00400000, 0xb023cd8b, ROMType::PROGRAM, 0 },
    { "222-s1.s1",    0x00020000, 0x8d3d3bf9, ROMType::TEXT, 0 },
    { "222-c1.c1",    0x00400000, 0x68f2ed95, ROMType::SPRITE, 0 },
    { "222-c2.c2",    0x00400000, 0xa6e9aff0, ROMType::SPRITE, 0 },
    { "222-c3.c3",    0x00400000, 0xc91b40f4, ROMType::SPRITE, 0 },
    { "222-c4.c4",    0x00400000, 0x359510a4, ROMType::SPRITE, 0 },
    { "222-c5.c5",    0x00400000, 0x9cfbb22d, ROMType::SPRITE, 0 },
    { "222-c6.c6",    0x00400000, 0x685efc32, ROMType::SPRITE, 0 },
    { "222-c7.c7",    0x00400000, 0xd0f86f0d, ROMType::SPRITE, 0 },
    { "222-c8.c8",    0x00400000, 0xadfc50e3, ROMType::SPRITE, 0 },
    { "222-m1.m1",    0x00020000, 0x7615bc1b, ROMType::SOUND_PROGRAM, 0 },
    { "222-v1.v1",    0x00400000, 0x7d6ba95f, ROMType::SOUND_SAMPLE, 0 },
    { "222-v2.v2",    0x00400000, 0x6c33bb5d, ROMType::SOUND_SAMPLE, 0 },
    { "222-v3.v3",    0x00200000, 0x831ea8c0, ROMType::SOUND_SAMPLE, 0 },
};

// Samurai Shodown V / Samurai Spirits Zero (Fully Decrypted)
static const ROMEntry samsh5fd_roms[] = {
    { "270-p1d.p1",    0x00400000, 0x4f8f86bd, ROMType::PROGRAM, 0 },
    { "270-p2d.sp2",   0x00400000, 0x91979dee, ROMType::PROGRAM, 0 },
    { "270-s1d.s1",    0x00020000, 0x2ad6048b, ROMType::TEXT, 0 },
    { "270-c1d.c1",    0x00800000, 0x9adec562, ROMType::SPRITE, 0 },
    { "270-c2d.c2",    0x00800000, 0xac0309e5, ROMType::SPRITE, 0 },
    { "270-c3d.c3",    0x00800000, 0x82db9dae, ROMType::SPRITE, 0 },
    { "270-c4d.c4",    0x00800000, 0xf8041153, ROMType::SPRITE, 0 },
    { "270-c5d.c5",    0x00800000, 0xe689d62d, ROMType::SPRITE, 0 },
    { "270-c6d.c6",    0x00800000, 0xa993bdcf, ROMType::SPRITE, 0 },
    { "270-c7d.c7",    0x00800000, 0x707d56a0, ROMType::SPRITE, 0 },
    { "270-c8d.c8",    0x00800000, 0xf5903adc, ROMType::SPRITE, 0 },
    { "270-m1d.m1",    0x00020000, 0xf119997c, ROMType::SOUND_PROGRAM, 0 },
    { "270-v1d.v1",    0x00800000, 0x809c7617, ROMType::SOUND_SAMPLE, 0 },
    { "270-v2d.v2",    0x00800000, 0x42671607, ROMType::SOUND_SAMPLE, 0 },
};

// Samurai Shodown V / Samurai Spirits Zero (NGM-2700, set 1)
static const ROMEntry samsho5_roms[] = {
    { "270-p1.p1",    0x00400000, 0x4a2a09e6, ROMType::PROGRAM, 0 },
    { "270-p2.sp2",   0x00400000, 0xe0c74c85, ROMType::PROGRAM, 0 },
    { "270-c1.c1",    0x00800000, 0x14ffffac, ROMType::SPRITE, 0 },
    { "270-c2.c2",    0x00800000, 0x401f7299, ROMType::SPRITE, 0 },
    { "270-c3.c3",    0x00800000, 0x838f0260, ROMType::SPRITE, 0 },
    { "270-c4.c4",    0x00800000, 0x041560a5, ROMType::SPRITE, 0 },
    { "270-c5.c5",    0x00800000, 0xbd30b52d, ROMType::SPRITE, 0 },
    { "270-c6.c6",    0x00800000, 0x86a69c70, ROMType::SPRITE, 0 },
    { "270-c7.c7",    0x00800000, 0xd28fbc3c, ROMType::SPRITE, 0 },
    { "270-c8.c8",    0x00800000, 0x02c530a6, ROMType::SPRITE, 0 },
    { "270-m1.m1",    0x00080000, 0x49c9901a, ROMType::SOUND_PROGRAM, 0 },
    { "270-v1.v1",    0x00800000, 0x62e434eb, ROMType::SOUND_SAMPLE, 0 },
    { "270-v2.v2",    0x00800000, 0x180f3c9a, ROMType::SOUND_SAMPLE, 0 },
};

// Samurai Shodown V Special / Samurai Spirits Zero Special (Fully Decrypted)
static const ROMEntry ss5spfd_roms[] = {
    { "272-p1d.p1",    0x00400000, 0xd5d492a9, ROMType::PROGRAM, 0 },
    { "272-p2d.sp2",   0x00400000, 0xb85f2c0f, ROMType::PROGRAM, 0 },
    { "272-s1d.s1",    0x00020000, 0xc297f973, ROMType::TEXT, 0 },
    { "272-c1d.c1",    0x00800000, 0x8548097e, ROMType::SPRITE, 0 },
    { "272-c2d.c2",    0x00800000, 0x8c1b48d0, ROMType::SPRITE, 0 },
    { "272-c3d.c3",    0x00800000, 0x96ddb28c, ROMType::SPRITE, 0 },
    { "272-c4d.c4",    0x00800000, 0x99ef7a0a, ROMType::SPRITE, 0 },
    { "272-c5d.c5",    0x00800000, 0x772e8b1e, ROMType::SPRITE, 0 },
    { "272-c6d.c6",    0x00800000, 0x5fff21fc, ROMType::SPRITE, 0 },
    { "272-c7d.c7",    0x00800000, 0x9ac56a0e, ROMType::SPRITE, 0 },
    { "272-c8d.c8",    0x00800000, 0xcfde7aff, ROMType::SPRITE, 0 },
    { "272-m1d.m1",    0x00020000, 0x654e9236, ROMType::SOUND_PROGRAM, 0 },
    { "272-v1d.v1",    0x00800000, 0x28d57d10, ROMType::SOUND_SAMPLE, 0 },
    { "272-v2d.v2",    0x00800000, 0x95fe7646, ROMType::SOUND_SAMPLE, 0 },
};

// Samurai Shodown V Special / Samurai Spirits Zero Special (NGM-2720)
static const ROMEntry samsh5sp_roms[] = {
    { "272-p1.p1",    0x00400000, 0xfb7a6bba, ROMType::PROGRAM, 0 },
    { "272-p2.sp2",   0x00400000, 0x63492ea6, ROMType::PROGRAM, 0 },
    { "272-c1.c1",    0x00800000, 0x4f97661a, ROMType::SPRITE, 0 },
    { "272-c2.c2",    0x00800000, 0xa3afda4f, ROMType::SPRITE, 0 },
    { "272-c3.c3",    0x00800000, 0x8c3c7502, ROMType::SPRITE, 0 },
    { "272-c4.c4",    0x00800000, 0x32d5e2e2, ROMType::SPRITE, 0 },
    { "272-c5.c5",    0x00800000, 0x6ce085bc, ROMType::SPRITE, 0 },
    { "272-c6.c6",    0x00800000, 0x05c8dc8e, ROMType::SPRITE, 0 },
    { "272-c7.c7",    0x00800000, 0x1417b742, ROMType::SPRITE, 0 },
    { "272-c8.c8",    0x00800000, 0xd49773cd, ROMType::SPRITE, 0 },
    { "272-m1.m1",    0x00080000, 0xadeebf40, ROMType::SOUND_PROGRAM, 0 },
    { "272-v1.v1",    0x00800000, 0x76a94127, ROMType::SOUND_SAMPLE, 0 },
    { "272-v2.v2",    0x00800000, 0x4ba507f1, ROMType::SOUND_SAMPLE, 0 },
};

// Savage Reign / Fu'un Mokushiroku - Kakutou Sousei
static const ROMEntry savagere_roms[] = {
    { "059-p1.p1",    0x00200000, 0x01d4e9c0, ROMType::PROGRAM, 0 },
    { "059-s1.s1",    0x00020000, 0xe08978ca, ROMType::TEXT, 0 },
    { "059-c1.c1",    0x00200000, 0x763ba611, ROMType::SPRITE, 0 },
    { "059-c2.c2",    0x00200000, 0xe05e8ca6, ROMType::SPRITE, 0 },
    { "059-c3.c3",    0x00200000, 0x3e4eba4b, ROMType::SPRITE, 0 },
    { "059-c4.c4",    0x00200000, 0x3c2a3808, ROMType::SPRITE, 0 },
    { "059-c5.c5",    0x00200000, 0x59013f9e, ROMType::SPRITE, 0 },
    { "059-c6.c6",    0x00200000, 0x1c8d5def, ROMType::SPRITE, 0 },
    { "059-c7.c7",    0x00200000, 0xc88f7035, ROMType::SPRITE, 0 },
    { "059-c8.c8",    0x00200000, 0x484ce3ba, ROMType::SPRITE, 0 },
    { "059-m1.m1",    0x00020000, 0x29992eba, ROMType::SOUND_PROGRAM, 0 },
    { "059-v1.v1",    0x00200000, 0x530c50fd, ROMType::SOUND_SAMPLE, 0 },
    { "059-v2.v2",    0x00200000, 0xeb6f1cdb, ROMType::SOUND_SAMPLE, 0 },
    { "059-v3.v3",    0x00200000, 0x7038c2f9, ROMType::SOUND_SAMPLE, 0 },
};

// Sengoku / Sengoku Denshou (NGM-017 ~ NGH-017)
static const ROMEntry sengoku_roms[] = {
    { "017-p1.p1",    0x00080000, 0xf8a63983, ROMType::PROGRAM, 0 },
    { "017-p2.p2",    0x00020000, 0x3024bbb3, ROMType::PROGRAM, 0 },
    { "017-s1.s1",    0x00020000, 0xb246204d, ROMType::TEXT, 0 },
    { "017-c1.c1",    0x00100000, 0xb4eb82a1, ROMType::SPRITE, 0 },
    { "017-c2.c2",    0x00100000, 0xd55c550d, ROMType::SPRITE, 0 },
    { "017-c3.c3",    0x00100000, 0xed51ef65, ROMType::SPRITE, 0 },
    { "017-c4.c4",    0x00100000, 0xf4f3c9cb, ROMType::SPRITE, 0 },
    { "017-m1.m1",    0x00020000, 0x9b4f34c6, ROMType::SOUND_PROGRAM, 0 },
    { "017-v1.v1",    0x00100000, 0x23663295, ROMType::SOUND_SAMPLE, 0 },
    { "017-v2.v2",    0x00100000, 0xf61e6765, ROMType::SOUND_SAMPLE, 0 },
};

// Sengoku 2 / Sengoku Denshou 2
static const ROMEntry sengoku2_roms[] = {
    { "040-p1.p1",    0x00100000, 0x6dde02c2, ROMType::PROGRAM, 0 },
    { "040-s1.s1",    0x00020000, 0xcd9802a3, ROMType::TEXT, 0 },
    { "040-c1.c1",    0x00200000, 0xfaa8ea99, ROMType::SPRITE, 0 },
    { "040-c2.c2",    0x00200000, 0x87d0ec65, ROMType::SPRITE, 0 },
    { "040-c3.c3",    0x00080000, 0x24b5ba80, ROMType::SPRITE, 0 },
    { "040-c4.c4",    0x00080000, 0x1c9e9930, ROMType::SPRITE, 0 },
    { "040-m1.m1",    0x00020000, 0xd4de4bca, ROMType::SOUND_PROGRAM, 0 },
    { "040-v1.v1",    0x00200000, 0x71cb4b5d, ROMType::SOUND_SAMPLE, 0 },
    { "040-v2.v2",    0x00100000, 0xc5cece01, ROMType::SOUND_SAMPLE, 0 },
};

// Sengoku 3 / Sengoku Densho 2001 (Fully Decrypted)
static const ROMEntry sengk3fd_roms[] = {
    { "261-ph1d.p1",  0x00100000, 0x9295cc2c, ROMType::PROGRAM, 0 },
    { "261-p2d.sp2",  0x00100000, 0xc74e374c, ROMType::PROGRAM, 0 },
    { "261-s1d.s1",   0x00020000, 0xc1e27cc7, ROMType::TEXT, 0 },
    { "261-c1d.c1",   0x00800000, 0x9af7cbca, ROMType::SPRITE, 0 },
    { "261-c2d.c2",   0x00800000, 0x2a1f874d, ROMType::SPRITE, 0 },
    { "261-c3d.c3",   0x00800000, 0x5403adb5, ROMType::SPRITE, 0 },
    { "261-c4d.c4",   0x00800000, 0x18926df6, ROMType::SPRITE, 0 },
    { "261-m1.m1",    0x00080000, 0x7d501c39, ROMType::SOUND_PROGRAM, 0 },
    { "261-v1.v1",    0x00400000, 0x64c30081, ROMType::SOUND_SAMPLE, 0 },
    { "261-v2.v2",    0x00400000, 0x392a9c47, ROMType::SOUND_SAMPLE, 0 },
    { "261-v3.v3",    0x00400000, 0xc1a7ebe3, ROMType::SOUND_SAMPLE, 0 },
    { "261-v4.v4",    0x00200000, 0x9000d085, ROMType::SOUND_SAMPLE, 0 },
};

// Sengoku 3 / Sengoku Densho 2001 (set 1)
static const ROMEntry sengoku3_roms[] = {
    { "261-ph1.p1",   0x00200000, 0xe0d4bc0a, ROMType::PROGRAM, 0 },
    { "261-c1.c1",    0x00800000, 0xded84d9c, ROMType::SPRITE, 0 },
    { "261-c2.c2",    0x00800000, 0xb8eb4348, ROMType::SPRITE, 0 },
    { "261-c3.c3",    0x00800000, 0x84e2034a, ROMType::SPRITE, 0 },
    { "261-c4.c4",    0x00800000, 0x0b45ae53, ROMType::SPRITE, 0 },
    { "261-m1.m1",    0x00080000, 0x7d501c39, ROMType::SOUND_PROGRAM, 0 },
    { "261-v1.v1",    0x00400000, 0x64c30081, ROMType::SOUND_SAMPLE, 0 },
    { "261-v2.v2",    0x00400000, 0x392a9c47, ROMType::SOUND_SAMPLE, 0 },
    { "261-v3.v3",    0x00400000, 0xc1a7ebe3, ROMType::SOUND_SAMPLE, 0 },
    { "261-v4.v4",    0x00200000, 0x9000d085, ROMType::SOUND_SAMPLE, 0 },
};

// Shock Troopers (set 1)
static const ROMEntry shocktro_roms[] = {
    { "238-pg1.p1",   0x00100000, 0xefedf8dc, ROMType::PROGRAM, 0 },
    { "238-p2.sp2",   0x00400000, 0x5b4a09c5, ROMType::PROGRAM, 0 },
    { "238-s1.s1",    0x00020000, 0x1f95cedb, ROMType::TEXT, 0 },
    { "238-c1.c1",    0x00400000, 0x90c6a181, ROMType::SPRITE, 0 },
    { "238-c2.c2",    0x00400000, 0x888720f0, ROMType::SPRITE, 0 },
    { "238-c3.c3",    0x00400000, 0x2c393aa3, ROMType::SPRITE, 0 },
    { "238-c4.c4",    0x00400000, 0xb9e909eb, ROMType::SPRITE, 0 },
    { "238-c5.c5",    0x00400000, 0xc22c68eb, ROMType::SPRITE, 0 },
    { "238-c6.c6",    0x00400000, 0x119323cd, ROMType::SPRITE, 0 },
    { "238-c7.c7",    0x00400000, 0xa72ce7ed, ROMType::SPRITE, 0 },
    { "238-c8.c8",    0x00400000, 0x1c7c2efb, ROMType::SPRITE, 0 },
    { "238-m1.m1",    0x00020000, 0x075b9518, ROMType::SOUND_PROGRAM, 0 },
    { "238-v1.v1",    0x00400000, 0x260c0bef, ROMType::SOUND_SAMPLE, 0 },
    { "238-v2.v2",    0x00200000, 0x4ad7d59e, ROMType::SOUND_SAMPLE, 0 },
};

// Shock Troopers - 2nd Squad
static const ROMEntry shocktr2_roms[] = {
    { "246-p1.p1",    0x00100000, 0x6d4b7781, ROMType::PROGRAM, 0 },
    { "246-p2.sp2",   0x00400000, 0x72ea04c3, ROMType::PROGRAM, 0 },
    { "246-s1.s1",    0x00020000, 0x2a360637, ROMType::TEXT, 0 },
    { "246-c1.c1",    0x00800000, 0x47ac9ec5, ROMType::SPRITE, 0 },
    { "246-c2.c2",    0x00800000, 0x7bcab64f, ROMType::SPRITE, 0 },
    { "246-c3.c3",    0x00800000, 0xdb2f73e8, ROMType::SPRITE, 0 },
    { "246-c4.c4",    0x00800000, 0x5503854e, ROMType::SPRITE, 0 },
    { "246-c5.c5",    0x00800000, 0x055b3701, ROMType::SPRITE, 0 },
    { "246-c6.c6",    0x00800000, 0x7e2caae1, ROMType::SPRITE, 0 },
    { "246-m1.m1",    0x00020000, 0xd0604ad1, ROMType::SOUND_PROGRAM, 0 },
    { "246-v1.v1",    0x00400000, 0x16986fc6, ROMType::SOUND_SAMPLE, 0 },
    { "246-v2.v2",    0x00400000, 0xada41e83, ROMType::SOUND_SAMPLE, 0 },
    { "246-v3.v3",    0x00200000, 0xa05ba5db, ROMType::SOUND_SAMPLE, 0 },
};

// Shougi no Tatsujin - Master of Shougi
static const ROMEntry moshougi_roms[] = {
    { "203-p1.p1",    0x00100000, 0x7ba70e2d, ROMType::PROGRAM, 0 },
    { "203-s1.s1",    0x00020000, 0xbfdc8309, ROMType::TEXT, 0 },
    { "203-c1.c1",    0x00200000, 0xbba9e8c0, ROMType::SPRITE, 0 },
    { "203-c2.c2",    0x00200000, 0x2574be03, ROMType::SPRITE, 0 },
    { "203-m1.m1",    0x00020000, 0xa602c2c2, ROMType::SOUND_PROGRAM, 0 },
    { "203-v1.v1",    0x00200000, 0xbaa2b9a5, ROMType::SOUND_SAMPLE, 0 },
};

// SNK vs. Capcom - SVC Chaos (Fully Decrypted)
static const ROMEntry svcfd_roms[] = {
    { "269-p1d.p1",    0x00600000, 0x93855c0b, ROMType::PROGRAM, 0 },
    { "269-s1d.s1",    0x00080000, 0xad184232, ROMType::TEXT, 0 },
    { "269-c1d.c1",    0x00800000, 0x465d473b, ROMType::SPRITE, 0 },
    { "269-c2d.c2",    0x00800000, 0x3eb28f78, ROMType::SPRITE, 0 },
    { "269-c3d.c3",    0x00800000, 0xf4d4ab2b, ROMType::SPRITE, 0 },
    { "269-c4d.c4",    0x00800000, 0xa69d523a, ROMType::SPRITE, 0 },
    { "269-c5d.c5",    0x00800000, 0xba2a7892, ROMType::SPRITE, 0 },
    { "269-c6d.c6",    0x00800000, 0x37371ca1, ROMType::SPRITE, 0 },
    { "269-c7d.c7",    0x00800000, 0x5595b6cc, ROMType::SPRITE, 0 },
    { "269-c8d.c8",    0x00800000, 0xb17dfcf9, ROMType::SPRITE, 0 },
    { "269-m1d.m1",    0x00020000, 0x447b3123, ROMType::SOUND_PROGRAM, 0 },
    { "269-v1d.v1",    0x00800000, 0xff64cd56, ROMType::SOUND_SAMPLE, 0 },
    { "269-v2d.v2",    0x00800000, 0xa8dd6446, ROMType::SOUND_SAMPLE, 0 },
};

// SNK vs. Capcom - SVC Chaos (NGM-2690 ~ NGH-2690)
static const ROMEntry svc_roms[] = {
    { "269-p1.p1",    0x00400000, 0x38e2005e, ROMType::PROGRAM, 0 },
    { "269-p2.p2",    0x00400000, 0x6d13797c, ROMType::PROGRAM, 0 },
    { "269-c1r.c1",   0x00800000, 0x887b4068, ROMType::SPRITE, 0 },
    { "269-c2r.c2",   0x00800000, 0x4e8903e4, ROMType::SPRITE, 0 },
    { "269-c3r.c3",   0x00800000, 0x7d9c55b0, ROMType::SPRITE, 0 },
    { "269-c4r.c4",   0x00800000, 0x8acb5bb6, ROMType::SPRITE, 0 },
    { "269-c5r.c5",   0x00800000, 0x097a4157, ROMType::SPRITE, 0 },
    { "269-c6r.c6",   0x00800000, 0xe19df344, ROMType::SPRITE, 0 },
    { "269-c7r.c7",   0x00800000, 0xd8f0340b, ROMType::SPRITE, 0 },
    { "269-c8r.c8",   0x00800000, 0x2570b71b, ROMType::SPRITE, 0 },
    { "269-m1.m1",    0x00080000, 0xf6819d00, ROMType::SOUND_PROGRAM, 0 },
    { "269-v1.v1",    0x00800000, 0xc659b34c, ROMType::SOUND_SAMPLE, 0 },
    { "269-v2.v2",    0x00800000, 0xdd903835, ROMType::SOUND_SAMPLE, 0 },
};

// Soccer Brawl (NGM-031)
static const ROMEntry socbrawl_roms[] = {
    { "031-pg1.p1",   0x00080000, 0x17f034a7, ROMType::PROGRAM, 0 },
    { "031-s1.s1",    0x00020000, 0x4c117174, ROMType::TEXT, 0 },
    { "031-c1.c1",    0x00100000, 0xbd0a4eb8, ROMType::SPRITE, 0 },
    { "031-c2.c2",    0x00100000, 0xefde5382, ROMType::SPRITE, 0 },
    { "031-c3.c3",    0x00080000, 0x580f7f33, ROMType::SPRITE, 0 },
    { "031-c4.c4",    0x00080000, 0xed297de8, ROMType::SPRITE, 0 },
    { "031-m1.m1",    0x00020000, 0xcb37427c, ROMType::SOUND_PROGRAM, 0 },
    { "031-v1.v1",    0x00100000, 0xcc78497e, ROMType::SOUND_SAMPLE, 0 },
    { "031-v2.v2",    0x00100000, 0xdda043c6, ROMType::SOUND_SAMPLE, 0 },
};

// Spin Master / Miracle Adventure
static const ROMEntry spinmast_roms[] = {
    { "062-p1.p1",    0x00100000, 0x37aba1aa, ROMType::PROGRAM, 0 },
    { "062-p2.sp2",   0x00100000, 0xf025ab77, ROMType::PROGRAM, 0 },
    { "062-s1.s1",    0x00020000, 0x289e2bbe, ROMType::TEXT, 0 },
    { "062-c1.c1",    0x00100000, 0xa9375aa2, ROMType::SPRITE, 0 },
    { "062-c2.c2",    0x00100000, 0x0e73b758, ROMType::SPRITE, 0 },
    { "062-c3.c3",    0x00100000, 0xdf51e465, ROMType::SPRITE, 0 },
    { "062-c4.c4",    0x00100000, 0x38517e90, ROMType::SPRITE, 0 },
    { "062-c5.c5",    0x00100000, 0x7babd692, ROMType::SPRITE, 0 },
    { "062-c6.c6",    0x00100000, 0xcde5ade5, ROMType::SPRITE, 0 },
    { "062-c7.c7",    0x00100000, 0xbb2fd7c0, ROMType::SPRITE, 0 },
    { "062-c8.c8",    0x00100000, 0x8d7be933, ROMType::SPRITE, 0 },
    { "062-m1.m1",    0x00020000, 0x76108b2f, ROMType::SOUND_PROGRAM, 0 },
    { "062-v1.v1",    0x00100000, 0xcc281aef, ROMType::SOUND_SAMPLE, 0 },
};

// Stakes Winner / Stakes Winner - GI Kinzen Seiha e no Michi
static const ROMEntry stakwin_roms[] = {
    { "088-p1.p1",    0x00200000, 0xbd5814f6, ROMType::PROGRAM, 0 },
    { "088-s1.s1",    0x00020000, 0x073cb208, ROMType::TEXT, 0 },
    { "088-c1.c1",    0x00200000, 0x6e733421, ROMType::SPRITE, 0 },
    { "088-c2.c2",    0x00200000, 0x4d865347, ROMType::SPRITE, 0 },
    { "088-c3.c3",    0x00200000, 0x8fa5a9eb, ROMType::SPRITE, 0 },
    { "088-c4.c4",    0x00200000, 0x4604f0dc, ROMType::SPRITE, 0 },
    { "088-m1.m1",    0x00020000, 0x2fe1f499, ROMType::SOUND_PROGRAM, 0 },
    { "088-v1.v1",    0x00200000, 0xb7785023, ROMType::SOUND_SAMPLE, 0 },
};

// Stakes Winner 2
static const ROMEntry stakwin2_roms[] = {
    { "227-p1.p1",    0x00200000, 0xdaf101d2, ROMType::PROGRAM, 0 },
    { "227-s1.s1",    0x00020000, 0x2a8c4462, ROMType::TEXT, 0 },
    { "227-c1.c1",    0x00400000, 0x7d6c2af4, ROMType::SPRITE, 0 },
    { "227-c2.c2",    0x00400000, 0x7e402d39, ROMType::SPRITE, 0 },
    { "227-c3.c3",    0x00200000, 0x93dfd660, ROMType::SPRITE, 0 },
    { "227-c4.c4",    0x00200000, 0x7efea43a, ROMType::SPRITE, 0 },
    { "227-m1.m1",    0x00020000, 0xc8e5e0f9, ROMType::SOUND_PROGRAM, 0 },
    { "227-v1.v1",    0x00400000, 0xb8f24181, ROMType::SOUND_SAMPLE, 0 },
    { "227-v2.v2",    0x00400000, 0xee39e260, ROMType::SOUND_SAMPLE, 0 },
};

// Street Hoop / Street Slam / Dunk Dream (DEM-004 ~ DEH-004)
static const ROMEntry strhoop_roms[] = {
    { "079-p1.p1",    0x00100000, 0x5e78328e, ROMType::PROGRAM, 0 },
    { "079-s1.s1",    0x00020000, 0x3ac06665, ROMType::TEXT, 0 },
    { "079-c1.c1",    0x00200000, 0x0581c72a, ROMType::SPRITE, 0 },
    { "079-c2.c2",    0x00200000, 0x5b9b8fb6, ROMType::SPRITE, 0 },
    { "079-c3.c3",    0x00200000, 0xcd65bb62, ROMType::SPRITE, 0 },
    { "079-c4.c4",    0x00200000, 0xa4c90213, ROMType::SPRITE, 0 },
    { "079-m1.m1",    0x00020000, 0xbee3455a, ROMType::SOUND_PROGRAM, 0 },
    { "079-v1.v1",    0x00200000, 0x718a2400, ROMType::SOUND_SAMPLE, 0 },
    { "079-v2.v2",    0x00100000, 0x720774eb, ROMType::SOUND_SAMPLE, 0 },
};

// Strikers 1945 Plus
static const ROMEntry s1945p_roms[] = {
    { "254-p1.p1",    0x00100000, 0xff8efcff, ROMType::PROGRAM, 0 },
    { "254-p2.sp2",   0x00400000, 0xefdfd4dd, ROMType::PROGRAM, 0 },
    { "254-c1.c1",    0x00800000, 0xae6fc8ef, ROMType::SPRITE, 0 },
    { "254-c2.c2",    0x00800000, 0x436fa176, ROMType::SPRITE, 0 },
    { "254-c3.c3",    0x00800000, 0xe53ff2dc, ROMType::SPRITE, 0 },
    { "254-c4.c4",    0x00800000, 0x818672f0, ROMType::SPRITE, 0 },
    { "254-c5.c5",    0x00800000, 0x4580eacd, ROMType::SPRITE, 0 },
    { "254-c6.c6",    0x00800000, 0xe34970fc, ROMType::SPRITE, 0 },
    { "254-c7.c7",    0x00800000, 0xf2323239, ROMType::SPRITE, 0 },
    { "254-c8.c8",    0x00800000, 0x66848c7d, ROMType::SPRITE, 0 },
    { "254-m1.m1",    0x00020000, 0x994b4487, ROMType::SOUND_PROGRAM, 0 },
    { "254-v1.v1",    0x00400000, 0x844f58fb, ROMType::SOUND_SAMPLE, 0 },
    { "254-v2.v2",    0x00400000, 0xd9a248f0, ROMType::SOUND_SAMPLE, 0 },
    { "254-v3.v3",    0x00400000, 0x0b0d2d33, ROMType::SOUND_SAMPLE, 0 },
    { "254-v4.v4",    0x00400000, 0x6d13dc91, ROMType::SOUND_SAMPLE, 0 },
};

// Super Dodge Ball / Kunio no Nekketsu Toukyuu Densetsu
static const ROMEntry sdodgeb_roms[] = {
    { "208-p1.p1",    0x00200000, 0x127f3d32, ROMType::PROGRAM, 0 },
    { "208-s1.s1",    0x00020000, 0x64abd6b3, ROMType::TEXT, 0 },
    { "208-c1.c1",    0x00400000, 0x93d8619b, ROMType::SPRITE, 0 },
    { "208-c2.c2",    0x00400000, 0x1c737bb6, ROMType::SPRITE, 0 },
    { "208-c3.c3",    0x00200000, 0x14cb1703, ROMType::SPRITE, 0 },
    { "208-c4.c4",    0x00200000, 0xc7165f19, ROMType::SPRITE, 0 },
    { "208-m1.m1",    0x00020000, 0x0a5f3325, ROMType::SOUND_PROGRAM, 0 },
    { "208-v1.v1",    0x00400000, 0xe7899a24, ROMType::SOUND_SAMPLE, 0 },
};

// Super Sidekicks / Tokuten Ou
static const ROMEntry ssideki_roms[] = {
    { "052-p1.p1",    0x00080000, 0x9cd97256, ROMType::PROGRAM, 0 },
    { "052-s1.s1",    0x00020000, 0x97689804, ROMType::TEXT, 0 },
    { "052-c1.c1",    0x00200000, 0x53e1c002, ROMType::SPRITE, 0 },
    { "052-c2.c2",    0x00200000, 0x776a2d1f, ROMType::SPRITE, 0 },
    { "052-m1.m1",    0x00020000, 0x49f17d2d, ROMType::SOUND_PROGRAM, 0 },
    { "052-v1.v1",    0x00200000, 0x22c097a5, ROMType::SOUND_SAMPLE, 0 },
};

// Super Sidekicks 2 - The World Championship / Tokuten Ou 2 - Real Fight Football (NGM-061 ~ NGH-061)
static const ROMEntry ssideki2_roms[] = {
    { "061-p1.p1",    0x00100000, 0x5969e0dc, ROMType::PROGRAM, 0 },
    { "061-s1.s1",    0x00020000, 0x226d1b68, ROMType::TEXT, 0 },
    { "061-c1-16.c1", 0x00200000, 0xa626474f, ROMType::SPRITE, 0 },
    { "061-c2-16.c2", 0x00200000, 0xc3be42ae, ROMType::SPRITE, 0 },
    { "061-c3-16.c3", 0x00200000, 0x2a7b98b9, ROMType::SPRITE, 0 },
    { "061-c4-16.c4", 0x00200000, 0xc0be9a1f, ROMType::SPRITE, 0 },
    { "061-m1.m1",    0x00020000, 0x156f6951, ROMType::SOUND_PROGRAM, 0 },
    { "061-v1.v1",    0x00200000, 0xf081c8d3, ROMType::SOUND_SAMPLE, 0 },
    { "061-v2.v2",    0x00200000, 0x7cd63302, ROMType::SOUND_SAMPLE, 0 },
};

// Super Sidekicks 3 - The Next Glory / Tokuten Ou 3 - Eikou e no Chousen
static const ROMEntry ssideki3_roms[] = {
    { "081-p1.p1",    0x00200000, 0x6bc27a3d, ROMType::PROGRAM, 0 },
    { "081-s1.s1",    0x00020000, 0x7626da34, ROMType::TEXT, 0 },
    { "081-c1.c1",    0x00200000, 0x1fb68ebe, ROMType::SPRITE, 0 },
    { "081-c2.c2",    0x00200000, 0xb28d928f, ROMType::SPRITE, 0 },
    { "081-c3.c3",    0x00200000, 0x3b2572e8, ROMType::SPRITE, 0 },
    { "081-c4.c4",    0x00200000, 0x47d26a7c, ROMType::SPRITE, 0 },
    { "081-c5.c5",    0x00200000, 0x17d42f0d, ROMType::SPRITE, 0 },
    { "081-c6.c6",    0x00200000, 0x6b53fb75, ROMType::SPRITE, 0 },
    { "081-m1.m1",    0x00020000, 0x82fcd863, ROMType::SOUND_PROGRAM, 0 },
    { "081-v1.v1",    0x00200000, 0x201fa1e1, ROMType::SOUND_SAMPLE, 0 },
    { "081-v2.v2",    0x00200000, 0xacf29d96, ROMType::SOUND_SAMPLE, 0 },
    { "081-v3.v3",    0x00200000, 0xe524e415, ROMType::SOUND_SAMPLE, 0 },
};

// Tecmo World Soccer '96
static const ROMEntry twsoc96_roms[] = {
    { "086-p1.p1",    0x00100000, 0x03e20ab6, ROMType::PROGRAM, 0 },
    { "086-s1.s1",    0x00020000, 0x6f5e2b3a, ROMType::TEXT, 0 },
    { "086-c1.c1",    0x00400000, 0x2611bc2a, ROMType::SPRITE, 0 },
    { "086-c2.c2",    0x00400000, 0x6b0d6827, ROMType::SPRITE, 0 },
    { "086-c3.c3",    0x00100000, 0x750ddc0c, ROMType::SPRITE, 0 },
    { "086-c4.c4",    0x00100000, 0x7a6e7d82, ROMType::SPRITE, 0 },
    { "086-m1.m1",    0x00020000, 0xcb82bc5d, ROMType::SOUND_PROGRAM, 0 },
    { "086-v1.v1",    0x00200000, 0x97bf1986, ROMType::SOUND_SAMPLE, 0 },
    { "086-v2.v2",    0x00200000, 0xb7eb05df, ROMType::SOUND_SAMPLE, 0 },
};

// The Irritating Maze / Ultra Denryu Iraira Bou
static const ROMEntry irrmaze_roms[] = {
    { "236-p1.p1",    0x00200000, 0x4c2ff660, ROMType::PROGRAM, 0 },
    { "236-s1.s1",    0x00020000, 0x5d1ca640, ROMType::TEXT, 0 },
    { "236-c1.c1",    0x00400000, 0xc1d47902, ROMType::SPRITE, 0 },
    { "236-c2.c2",    0x00400000, 0xe15f972e, ROMType::SPRITE, 0 },
    { "236-m1.m1",    0x00020000, 0x880a1abd, ROMType::SOUND_PROGRAM, 0 },
    { "236-v1.v1",    0x00200000, 0x5f89c3b4, ROMType::SOUND_SAMPLE, 0 },
    { "236-v2.v2",    0x00100000, 0x72e3add7, ROMType::SOUND_SAMPLE, 0 },
};

// The King of Fighters '94 (NGM-055 ~ NGH-055)
static const ROMEntry kof94_roms[] = {
    { "055-p1.p1",    0x00200000, 0xf10a2042, ROMType::PROGRAM, 0 },
    { "055-s1.s1",    0x00020000, 0x825976c1, ROMType::TEXT, 0 },
    { "055-c1.c1",    0x00200000, 0xb96ef460, ROMType::SPRITE, 0 },
    { "055-c2.c2",    0x00200000, 0x15e096a7, ROMType::SPRITE, 0 },
    { "055-c3.c3",    0x00200000, 0x54f66254, ROMType::SPRITE, 0 },
    { "055-c4.c4",    0x00200000, 0x0b01765f, ROMType::SPRITE, 0 },
    { "055-c5.c5",    0x00200000, 0xee759363, ROMType::SPRITE, 0 },
    { "055-c6.c6",    0x00200000, 0x498da52c, ROMType::SPRITE, 0 },
    { "055-c7.c7",    0x00200000, 0x62f66888, ROMType::SPRITE, 0 },
    { "055-c8.c8",    0x00200000, 0xfe0a235d, ROMType::SPRITE, 0 },
    { "055-m1.m1",    0x00020000, 0xf6e77cf5, ROMType::SOUND_PROGRAM, 0 },
    { "055-v1.v1",    0x00200000, 0x8889596d, ROMType::SOUND_SAMPLE, 0 },
    { "055-v2.v2",    0x00200000, 0x25022b27, ROMType::SOUND_SAMPLE, 0 },
    { "055-v3.v3",    0x00200000, 0x83cf32c0, ROMType::SOUND_SAMPLE, 0 },
};

// The King of Fighters '95 (NGM-084)
static const ROMEntry kof95_roms[] = {
    { "084-p1.p1",    0x00200000, 0x2cba2716, ROMType::PROGRAM, 0 },
    { "084-s1.s1",    0x00020000, 0xde716f8a, ROMType::TEXT, 0 },
    { "084-c1.c1",    0x00400000, 0xfe087e32, ROMType::SPRITE, 0 },
    { "084-c2.c2",    0x00400000, 0x07864e09, ROMType::SPRITE, 0 },
    { "084-c3.c3",    0x00400000, 0xa4e65d1b, ROMType::SPRITE, 0 },
    { "084-c4.c4",    0x00400000, 0xc1ace468, ROMType::SPRITE, 0 },
    { "084-c5.c5",    0x00200000, 0x8a2c1edc, ROMType::SPRITE, 0 },
    { "084-c6.c6",    0x00200000, 0xf593ac35, ROMType::SPRITE, 0 },
    { "084-c7.c7",    0x00100000, 0x9904025f, ROMType::SPRITE, 0 },
    { "084-c8.c8",    0x00100000, 0x78eb0f9b, ROMType::SPRITE, 0 },
    { "084-m1.m1",    0x00020000, 0x6f2d7429, ROMType::SOUND_PROGRAM, 0 },
    { "084-v1.v1",    0x00400000, 0x84861b56, ROMType::SOUND_SAMPLE, 0 },
    { "084-v2.v2",    0x00200000, 0xb38a2803, ROMType::SOUND_SAMPLE, 0 },
    { "084-v3.v3",    0x00100000, 0xd683a338, ROMType::SOUND_SAMPLE, 0 },
};

// The King of Fighters '96 (NGM-214)
static const ROMEntry kof96_roms[] = {
    { "214-p1.p1",    0x00100000, 0x52755d74, ROMType::PROGRAM, 0 },
    { "214-p2.sp2",   0x00200000, 0x002ccb73, ROMType::PROGRAM, 0 },
    { "214-s1.s1",    0x00020000, 0x1254cbdb, ROMType::TEXT, 0 },
    { "214-c1.c1",    0x00400000, 0x7ecf4aa2, ROMType::SPRITE, 0 },
    { "214-c2.c2",    0x00400000, 0x05b54f37, ROMType::SPRITE, 0 },
    { "214-c3.c3",    0x00400000, 0x64989a65, ROMType::SPRITE, 0 },
    { "214-c4.c4",    0x00400000, 0xafbea515, ROMType::SPRITE, 0 },
    { "214-c5.c5",    0x00400000, 0x2a3bbd26, ROMType::SPRITE, 0 },
    { "214-c6.c6",    0x00400000, 0x44d30dc7, ROMType::SPRITE, 0 },
    { "214-c7.c7",    0x00400000, 0x3687331b, ROMType::SPRITE, 0 },
    { "214-c8.c8",    0x00400000, 0xfa1461ad, ROMType::SPRITE, 0 },
    { "214-m1.m1",    0x00020000, 0xdabc427c, ROMType::SOUND_PROGRAM, 0 },
    { "214-v1.v1",    0x00400000, 0x63f7b045, ROMType::SOUND_SAMPLE, 0 },
    { "214-v2.v2",    0x00400000, 0x25929059, ROMType::SOUND_SAMPLE, 0 },
    { "214-v3.v3",    0x00200000, 0x92a2257d, ROMType::SOUND_SAMPLE, 0 },
};

// The King of Fighters '97 (NGM-2320)
static const ROMEntry kof97_roms[] = {
    { "232-p1.p1",    0x00100000, 0x7db81ad9, ROMType::PROGRAM, 0 },
    { "232-p2.sp2",   0x00400000, 0x158b23f6, ROMType::PROGRAM, 0 },
    { "232-s1.s1",    0x00020000, 0x8514ecf5, ROMType::TEXT, 0 },
    { "232-c1.c1",    0x00800000, 0x5f8bf0a1, ROMType::SPRITE, 0 },
    { "232-c2.c2",    0x00800000, 0xe4d45c81, ROMType::SPRITE, 0 },
    { "232-c3.c3",    0x00800000, 0x581d6618, ROMType::SPRITE, 0 },
    { "232-c4.c4",    0x00800000, 0x49bb1e68, ROMType::SPRITE, 0 },
    { "232-c5.c5",    0x00400000, 0x34fc4e51, ROMType::SPRITE, 0 },
    { "232-c6.c6",    0x00400000, 0x4ff4d47b, ROMType::SPRITE, 0 },
    { "232-m1.m1",    0x00020000, 0x45348747, ROMType::SOUND_PROGRAM, 0 },
    { "232-v1.v1",    0x00400000, 0x22a2b5b5, ROMType::SOUND_SAMPLE, 0 },
    { "232-v2.v2",    0x00400000, 0x2304e744, ROMType::SOUND_SAMPLE, 0 },
    { "232-v3.v3",    0x00400000, 0x759eb954, ROMType::SOUND_SAMPLE, 0 },
};

// The King of Fighters '98 - The Slugfest / King of Fighters '98 - Dream Match Never Ends (NGM-2420)
static const ROMEntry kof98_roms[] = {
    { "242-p1.p1",    0x00200000, 0x8893df89, ROMType::PROGRAM, 0 },
    { "242-p2.sp2",   0x00400000, 0x980aba4c, ROMType::PROGRAM, 0 },
    { "242-s1.s1",    0x00020000, 0x7f7b4805, ROMType::TEXT, 0 },
    { "242-c1.c1",    0x00800000, 0xe564ecd6, ROMType::SPRITE, 0 },
    { "242-c2.c2",    0x00800000, 0xbd959b60, ROMType::SPRITE, 0 },
    { "242-c3.c3",    0x00800000, 0x22127b4f, ROMType::SPRITE, 0 },
    { "242-c4.c4",    0x00800000, 0x0b4fa044, ROMType::SPRITE, 0 },
    { "242-c5.c5",    0x00800000, 0x9d10bed3, ROMType::SPRITE, 0 },
    { "242-c6.c6",    0x00800000, 0xda07b6a2, ROMType::SPRITE, 0 },
    { "242-c7.c7",    0x00800000, 0xf6d7a38a, ROMType::SPRITE, 0 },
    { "242-c8.c8",    0x00800000, 0xc823e045, ROMType::SPRITE, 0 },
    { "242-m1.m1",    0x00040000, 0x4ef7016b, ROMType::SOUND_PROGRAM, 0 },
    { "242-v1.v1",    0x00400000, 0xb9ea8051, ROMType::SOUND_SAMPLE, 0 },
    { "242-v2.v2",    0x00400000, 0xcc11106e, ROMType::SOUND_SAMPLE, 0 },
    { "242-v3.v3",    0x00400000, 0x044ea4e1, ROMType::SOUND_SAMPLE, 0 },
    { "242-v4.v4",    0x00400000, 0x7985ea30, ROMType::SOUND_SAMPLE, 0 },
};

// The King of Fighters '98 - The Slugfest / King of Fighters '98 - Dream Match Never Ends (NGH-2420)
static const ROMEntry kof98h_roms[] = {
    { "242-pn1.p1",   0x100000, 0x61ac868a, ROMType::PROGRAM, 0 },
    { "242-p2.sp2",   0x400000, 0x980aba4c, ROMType::PROGRAM, 0 },
    { "242-s1.s1",    0x020000, 0x7f7b4805, ROMType::TEXT, 0 },
    { "242-c1.c1",    0x800000, 0xe564ecd6, ROMType::SPRITE, 0 },
    { "242-c2.c2",    0x800000, 0xbd959b60, ROMType::SPRITE, 0 },
    { "242-c3.c3",    0x800000, 0x22127b4f, ROMType::SPRITE, 0 },
    { "242-c4.c4",    0x800000, 0x0b4fa044, ROMType::SPRITE, 0 },
    { "242-c5.c5",    0x800000, 0x9d10bed3, ROMType::SPRITE, 0 },
    { "242-c6.c6",    0x800000, 0xda07b6a2, ROMType::SPRITE, 0 },
    { "242-c7.c7",    0x800000, 0xf6d7a38a, ROMType::SPRITE, 0 },
    { "242-c8.c8",    0x800000, 0xc823e045, ROMType::SPRITE, 0 },
    { "242-mg1.m1",   0x040000, 0x4e7a6b1b, ROMType::SOUND_PROGRAM, 0 },
    { "242-v1.v1",    0x400000, 0xb9ea8051, ROMType::SOUND_SAMPLE, 0 },
    { "242-v2.v2",    0x400000, 0xcc11106e, ROMType::SOUND_SAMPLE, 0 },
    { "242-v3.v3",    0x400000, 0x044ea4e1, ROMType::SOUND_SAMPLE, 0 },
    { "242-v4.v4",    0x400000, 0x7985ea30, ROMType::SOUND_SAMPLE, 0 },
};

// The King of Fighters '98 - The Slugfest / King of Fighters '98 - Dream Match Never Ends (Combo, Hack)
static const ROMEntry kof98cb_roms[] = {
    { "242-p1cb.p1",    0x100000, 0x2565e431, ROMType::PROGRAM, 0 },
    { "242-p2cb.sp2",   0x400000, 0xd34a4d38, ROMType::PROGRAM, 0 },
    { "242-s1cb.s1",    0x020000, 0x7333d8b0, ROMType::TEXT, 0 },
    { "242-c1cb.c1",    0x800000, 0x066db0a6, ROMType::SPRITE, 0 },
    { "242-c2cb.c2",    0x800000, 0x99d0b0fa, ROMType::SPRITE, 0 },
    { "242-c3cb.c3",    0x800000, 0xea84bdae, ROMType::SPRITE, 0 },
    { "242-c4cb.c4",    0x800000, 0x2c17ac8e, ROMType::SPRITE, 0 },
    { "242-c5.c5",      0x800000, 0x9d10bed3, ROMType::SPRITE, 0 },
    { "242-c6.c6",      0x800000, 0xda07b6a2, ROMType::SPRITE, 0 },
    { "242-c7.c7",      0x800000, 0xf6d7a38a, ROMType::SPRITE, 0 },
    { "242-c8.c8",      0x800000, 0xc823e045, ROMType::SPRITE, 0 },
    { "242-m1cb.m1",    0x040000, 0xdb046fc4, ROMType::SOUND_PROGRAM, 0 },
    { "242-v1.v1",      0x400000, 0xb9ea8051, ROMType::SOUND_SAMPLE, 0 },
    { "242-v2.v2",      0x400000, 0xcc11106e, ROMType::SOUND_SAMPLE, 0 },
    { "242-v3.v3",      0x400000, 0x044ea4e1, ROMType::SOUND_SAMPLE, 0 },
    { "242-v4.v4",      0x400000, 0x7985ea30, ROMType::SOUND_SAMPLE, 0 },
    /* GOTVG Combo - 20200328 */
    // { "242-p1cb.dif",   0x100000, 0xee8ec128, ROMType::PROGRAM, 0 },
    // { "242-s1cb.dif",   0x020000, 0x83ecd0f4, ROMType::TEXT, 0 },
};

// The King of Fighters '99 - Millennium Battle (Fully Decrypted)
static const ROMEntry kof99fd_roms[] = {
    { "152-p1.p1",    0x00100000, 0xf2c7ddfa, ROMType::PROGRAM, 0 },
    { "152-p2.sp2",   0x00400000, 0x274ef47a, ROMType::PROGRAM, 0 },
    { "251-s1d.s1",   0x00020000, 0x1b0133fe, ROMType::TEXT, 0 },
    { "251-c1d.c1",   0x00800000, 0xb3d88546, ROMType::SPRITE, 0 },
    { "251-c2d.c2",   0x00800000, 0x915c8634, ROMType::SPRITE, 0 },
    { "251-c3d.c3",   0x00800000, 0xb047c9d5, ROMType::SPRITE, 0 },
    { "251-c4d.c4",   0x00800000, 0x6bc8e4b1, ROMType::SPRITE, 0 },
    { "251-c5d.c5",   0x00800000, 0x9746268c, ROMType::SPRITE, 0 },
    { "251-c6d.c6",   0x00800000, 0x238b3e71, ROMType::SPRITE, 0 },
    { "251-c7d.c7",   0x00800000, 0x2f68fdeb, ROMType::SPRITE, 0 },
    { "251-c8d.c8",   0x00800000, 0x4c2fad1e, ROMType::SPRITE, 0 },
    { "251-m1.m1",    0x00020000, 0x5e74539c, ROMType::SOUND_PROGRAM, 0 },
    { "251-v1.v1",    0x00400000, 0xef2eecc8, ROMType::SOUND_SAMPLE, 0 },
    { "251-v2.v2",    0x00400000, 0x73e211ca, ROMType::SOUND_SAMPLE, 0 },
    { "251-v3.v3",    0x00400000, 0x821901da, ROMType::SOUND_SAMPLE, 0 },
    { "251-v4.v4",    0x00200000, 0xb49e6178, ROMType::SOUND_SAMPLE, 0 },
};

// The King of Fighters '99 - Millennium Battle (NGM-2510)
static const ROMEntry kof99_roms[] = {
    { "ka.neo-sma",   0x00040000, 0x7766d09e, ROMType::PROGRAM, 0 },
    { "251-p1.p1",    0x00400000, 0x006e4532, ROMType::PROGRAM, 0 },
    { "251-p2.p2",    0x00400000, 0x90175f15, ROMType::PROGRAM, 0 },
    { "251-c1.c1",    0x00800000, 0x0f9e93fe, ROMType::SPRITE, 0 },
    { "251-c2.c2",    0x00800000, 0xe71e2ea3, ROMType::SPRITE, 0 },
    { "251-c3.c3",    0x00800000, 0x238755d2, ROMType::SPRITE, 0 },
    { "251-c4.c4",    0x00800000, 0x438c8b22, ROMType::SPRITE, 0 },
    { "251-c5.c5",    0x00800000, 0x0b0abd0a, ROMType::SPRITE, 0 },
    { "251-c6.c6",    0x00800000, 0x65bbf281, ROMType::SPRITE, 0 },
    { "251-c7.c7",    0x00800000, 0xff65f62e, ROMType::SPRITE, 0 },
    { "251-c8.c8",    0x00800000, 0x8d921c68, ROMType::SPRITE, 0 },
    { "251-m1.m1",    0x00020000, 0x5e74539c, ROMType::SOUND_PROGRAM, 0 },
    { "251-v1.v1",    0x00400000, 0xef2eecc8, ROMType::SOUND_SAMPLE, 0 },
    { "251-v2.v2",    0x00400000, 0x73e211ca, ROMType::SOUND_SAMPLE, 0 },
    { "251-v3.v3",    0x00400000, 0x821901da, ROMType::SOUND_SAMPLE, 0 },
    { "251-v4.v4",    0x00200000, 0xb49e6178, ROMType::SOUND_SAMPLE, 0 },
};

// The King of Fighters 10th Anniversary (The King of Fighters 2002 bootleg / Fully Decrypted)
static const ROMEntry kof10thd_roms[] = {
    { "kf10-p1d.bin",    0x00800000, 0x30c82f4c, ROMType::PROGRAM, 0 },
    { "kf10-s1d.bin",    0x00020000, 0x3c757cb1, ROMType::TEXT, 0 },
    { "kf10-c1a.bin",    0x00400000, 0x3bbc0364, ROMType::SPRITE, 0 },
    { "kf10-c2a.bin",    0x00400000, 0x91230075, ROMType::SPRITE, 0 },
    { "kf10-c1b.bin",    0x00400000, 0xb5abfc28, ROMType::SPRITE, 0 },
    { "kf10-c2b.bin",    0x00400000, 0x6cc4c6e1, ROMType::SPRITE, 0 },
    { "kf10-c3a.bin",    0x00400000, 0x5b3d4a16, ROMType::SPRITE, 0 },
    { "kf10-c4a.bin",    0x00400000, 0xc6f3419b, ROMType::SPRITE, 0 },
    { "kf10-c3b.bin",    0x00400000, 0x9d2bba19, ROMType::SPRITE, 0 },
    { "kf10-c4b.bin",    0x00400000, 0x5a4050cb, ROMType::SPRITE, 0 },
    { "kf10-c5a.bin",    0x00400000, 0xa289d1e1, ROMType::SPRITE, 0 },
    { "kf10-c6a.bin",    0x00400000, 0xe6494b5d, ROMType::SPRITE, 0 },
    { "kf10-c5b.bin",    0x00400000, 0x404fff02, ROMType::SPRITE, 0 },
    { "kf10-c6b.bin",    0x00400000, 0xf2ccfc9e, ROMType::SPRITE, 0 },
    { "kf10-c7a.bin",    0x00400000, 0xbe79c5a8, ROMType::SPRITE, 0 },
    { "kf10-c8a.bin",    0x00400000, 0xa5952ca4, ROMType::SPRITE, 0 },
    { "kf10-c7b.bin",    0x00400000, 0x3fdb3542, ROMType::SPRITE, 0 },
    { "kf10-c8b.bin",    0x00400000, 0x661b7a52, ROMType::SPRITE, 0 },
    { "kf10-m1.bin",     0x00020000, 0xf6fab859, ROMType::SOUND_PROGRAM, 0 },
    { "kf10-v1.bin",     0x00800000, 0x0fc9a58d, ROMType::SOUND_SAMPLE, 0 },
    { "kf10-v2.bin",     0x00800000, 0xb8c475a4, ROMType::SOUND_SAMPLE, 0 },
};

// The King of Fighters 2000 (Fully Decrypted)
static const ROMEntry kof2kfd_roms[] = {
    { "257-pg1.p1",   0x00100000, 0x5f809dbe, ROMType::PROGRAM, 0 },
    { "257-pg2.sp2",  0x00400000, 0x693c2c5e, ROMType::PROGRAM, 0 },
    { "257-c1d.c1",   0x00800000, 0xabcdd424, ROMType::SPRITE, 0 },
    { "257-c2d.c2",   0x00800000, 0xcda33778, ROMType::SPRITE, 0 },
    { "257-c3d.c3",   0x00800000, 0x087fb15b, ROMType::SPRITE, 0 },
    { "257-c4d.c4",   0x00800000, 0xfe9dfde4, ROMType::SPRITE, 0 },
    { "257-c5d.c5",   0x00800000, 0x03ee4bf4, ROMType::SPRITE, 0 },
    { "257-c6d.c6",   0x00800000, 0x8599cc5b, ROMType::SPRITE, 0 },
    { "257-c7d.c7",   0x00800000, 0x71dfc3e2, ROMType::SPRITE, 0 },
    { "257-c8d.c8",   0x00800000, 0x0fa30e5f, ROMType::SPRITE, 0 },
    { "257-m1d.m1",   0x00040000, 0xd404db70, ROMType::SOUND_PROGRAM, 0 },
    { "257-v1.v1",    0x00400000, 0x17cde847, ROMType::SOUND_SAMPLE, 0 },
    { "257-v2.v2",    0x00400000, 0x1afb20ff, ROMType::SOUND_SAMPLE, 0 },
    { "257-v3.v3",    0x00400000, 0x4605036a, ROMType::SOUND_SAMPLE, 0 },
    { "257-v4.v4",    0x00400000, 0x764bbd6b, ROMType::SOUND_SAMPLE, 0 },
};

// The King of Fighters 2000 (NGM-2570 ~ NGH-2570)
static const ROMEntry kof2000_roms[] = {
    { "neo-sma",      0x00040000, 0x71c6e6bb, ROMType::PROGRAM, 0 },
    { "257-p1.p1",    0x00400000, 0x60947b4c, ROMType::PROGRAM, 0 },
    { "257-p2.p2",    0x00400000, 0x1b7ec415, ROMType::PROGRAM, 0 },
    { "257-c1.c1",    0x00800000, 0xcef1cdfa, ROMType::SPRITE, 0 },
    { "257-c2.c2",    0x00800000, 0xf7bf0003, ROMType::SPRITE, 0 },
    { "257-c3.c3",    0x00800000, 0x101e6560, ROMType::SPRITE, 0 },
    { "257-c4.c4",    0x00800000, 0xbd2fc1b1, ROMType::SPRITE, 0 },
    { "257-c5.c5",    0x00800000, 0x89775412, ROMType::SPRITE, 0 },
    { "257-c6.c6",    0x00800000, 0xfa7200d5, ROMType::SPRITE, 0 },
    { "257-c7.c7",    0x00800000, 0x7da11fe4, ROMType::SPRITE, 0 },
    { "257-c8.c8",    0x00800000, 0xb1afa60b, ROMType::SPRITE, 0 },
    { "257-m1.m1",    0x00040000, 0x4b749113, ROMType::SOUND_PROGRAM, 0 },
    { "257-v1.v1",    0x00400000, 0x17cde847, ROMType::SOUND_SAMPLE, 0 },
    { "257-v2.v2",    0x00400000, 0x1afb20ff, ROMType::SOUND_SAMPLE, 0 },
    { "257-v3.v3",    0x00400000, 0x4605036a, ROMType::SOUND_SAMPLE, 0 },
    { "257-v4.v4",    0x00400000, 0x764bbd6b, ROMType::SOUND_SAMPLE, 0 },
};

// The King of Fighters 2001 (Fully Decrypted)
static const ROMEntry kof2k1fd_roms[] = {
    { "262-pg1.p1",         0x00100000, 0x2af7e741, ROMType::PROGRAM, 0 },
    { "262-pg2.sp2",        0x00400000, 0x91eea062, ROMType::PROGRAM, 0 },
    { "262-s1d.s1",         0x00020000, 0x6d209796, ROMType::TEXT, 0 },
    { "262-c1d.c1",         0x00800000, 0x103225b1, ROMType::SPRITE, 0 },
    { "262-c2d.c2",         0x00800000, 0xf9d05d99, ROMType::SPRITE, 0 },
    { "262-c3d.c3",         0x00800000, 0x4c7ec427, ROMType::SPRITE, 0 },
    { "262-c4d.c4",         0x00800000, 0x1d237aa6, ROMType::SPRITE, 0 },
    { "262-c5d.c5",         0x00800000, 0xc2256db5, ROMType::SPRITE, 0 },
    { "262-c6d.c6",         0x00800000, 0x8d6565a9, ROMType::SPRITE, 0 },
    { "262-c7d.c7",         0x00800000, 0xd1408776, ROMType::SPRITE, 0 },
    { "262-c8d.c8",         0x00800000, 0x954d0e16, ROMType::SPRITE, 0 },
    { "262-m1d.m1",         0x00020000, 0x2fb0a8a5, ROMType::SOUND_PROGRAM, 0 },
    { "262-v1-08-e0.v1",    0x00400000, 0x83d49ecf, ROMType::SOUND_SAMPLE, 0 },
    { "262-v2-08-e0.v2",    0x00400000, 0x003f1843, ROMType::SOUND_SAMPLE, 0 },
    { "262-v3-08-e0.v3",    0x00400000, 0x2ae38dbe, ROMType::SOUND_SAMPLE, 0 },
    { "262-v4-08-e0.v4",    0x00400000, 0x26ec4dd9, ROMType::SOUND_SAMPLE, 0 },
};

// The King of Fighters 2001 (NGM-262?)
static const ROMEntry kof2001_roms[] = {
    { "262-p1-08-e0.p1",    0x00100000, 0x9381750d, ROMType::PROGRAM, 0 },
    { "262-p2-08-e0.sp2",   0x00400000, 0x8e0d8329, ROMType::PROGRAM, 0 },
    { "262-c1-08-e0.c1",    0x00800000, 0x99cc785a, ROMType::SPRITE, 0 },
    { "262-c2-08-e0.c2",    0x00800000, 0x50368cbf, ROMType::SPRITE, 0 },
    { "262-c3-08-e0.c3",    0x00800000, 0xfb14ff87, ROMType::SPRITE, 0 },
    { "262-c4-08-e0.c4",    0x00800000, 0x4397faf8, ROMType::SPRITE, 0 },
    { "262-c5-08-e0.c5",    0x00800000, 0x91f24be4, ROMType::SPRITE, 0 },
    { "262-c6-08-e0.c6",    0x00800000, 0xa31e4403, ROMType::SPRITE, 0 },
    { "262-c7-08-e0.c7",    0x00800000, 0x54d9d1ec, ROMType::SPRITE, 0 },
    { "262-c8-08-e0.c8",    0x00800000, 0x59289a6b, ROMType::SPRITE, 0 },
    { "265-262-m1.m1",      0x00040000, 0xa7f8119f, ROMType::SOUND_PROGRAM, 0 },
    { "262-v1-08-e0.v1",    0x00400000, 0x83d49ecf, ROMType::SOUND_SAMPLE, 0 },
    { "262-v2-08-e0.v2",    0x00400000, 0x003f1843, ROMType::SOUND_SAMPLE, 0 },
    { "262-v3-08-e0.v3",    0x00400000, 0x2ae38dbe, ROMType::SOUND_SAMPLE, 0 },
    { "262-v4-08-e0.v4",    0x00400000, 0x26ec4dd9, ROMType::SOUND_SAMPLE, 0 },
};

// The King of Fighters 2002 (Fully Decrypted)
static const ROMEntry kof2k2fd_roms[] = {
    { "265-p1.p1",     0x00100000, 0x9ede7323, ROMType::PROGRAM, 0 },
    { "265-p2d.sp2",   0x00400000, 0x432fdf53, ROMType::PROGRAM, 0 },
    { "265-s1d.s1",    0x00020000, 0xe0eaaba3, ROMType::TEXT, 0 },
    { "265-c1d.c1",    0x00800000, 0x7efa6ef7, ROMType::SPRITE, 0 },
    { "265-c2d.c2",    0x00800000, 0xaa82948b, ROMType::SPRITE, 0 },
    { "265-c3d.c3",    0x00800000, 0x959fad0b, ROMType::SPRITE, 0 },
    { "265-c4d.c4",    0x00800000, 0xefe6a468, ROMType::SPRITE, 0 },
    { "265-c5d.c5",    0x00800000, 0x74bba7c6, ROMType::SPRITE, 0 },
    { "265-c6d.c6",    0x00800000, 0xe20d2216, ROMType::SPRITE, 0 },
    { "265-c7d.c7",    0x00800000, 0x8a5b561c, ROMType::SPRITE, 0 },
    { "265-c8d.c8",    0x00800000, 0xbef667a3, ROMType::SPRITE, 0 },
    { "265-m1d.m1",    0x00020000, 0x1c661a4b, ROMType::SOUND_PROGRAM, 0 },
    { "265-v1d.v1",    0x00800000, 0x0fc9a58d, ROMType::SOUND_SAMPLE, 0 },
    { "265-v2d.v2",    0x00800000, 0xb8c475a4, ROMType::SOUND_SAMPLE, 0 },
};

// The King of Fighters 2002 (NGM-2650 ~ NGH-2650)
static const ROMEntry kof2002_roms[] = {
    { "265-p1.p1",    0x00100000, 0x9ede7323, ROMType::PROGRAM, 0 },
    { "265-p2.sp2",   0x00400000, 0x327266b8, ROMType::PROGRAM, 0 },
    { "265-c1.c1",    0x00800000, 0x2b65a656, ROMType::SPRITE, 0 },
    { "265-c2.c2",    0x00800000, 0xadf18983, ROMType::SPRITE, 0 },
    { "265-c3.c3",    0x00800000, 0x875e9fd7, ROMType::SPRITE, 0 },
    { "265-c4.c4",    0x00800000, 0x2da13947, ROMType::SPRITE, 0 },
    { "265-c5.c5",    0x00800000, 0x61bd165d, ROMType::SPRITE, 0 },
    { "265-c6.c6",    0x00800000, 0x03fdd1eb, ROMType::SPRITE, 0 },
    { "265-c7.c7",    0x00800000, 0x1a2749d8, ROMType::SPRITE, 0 },
    { "265-c8.c8",    0x00800000, 0xab0bb549, ROMType::SPRITE, 0 },
    { "265-m1.m1",    0x00020000, 0x85aaa632, ROMType::SOUND_PROGRAM, 0 },
    { "265-v1.v1",    0x00800000, 0x15e8f3f5, ROMType::SOUND_SAMPLE, 0 },
    { "265-v2.v2",    0x00800000, 0xda41d6f9, ROMType::SOUND_SAMPLE, 0 },
};

// The King of Fighters 2003 (Fully Decrypted)
static const ROMEntry kof2k3fd_roms[] = {
    { "271-p1d.p1",    0x00800000, 0x57a1981d, ROMType::PROGRAM, 0 },
    { "271-s1d.s1",    0x00080000, 0x3230e10f, ROMType::TEXT, 0 },
    { "271-c1d.c1",    0x00800000, 0xe42fc226, ROMType::SPRITE, 0 },
    { "271-c2d.c2",    0x00800000, 0x1b5e3b58, ROMType::SPRITE, 0 },
    { "271-c3d.c3",    0x00800000, 0xd334fdd9, ROMType::SPRITE, 0 },
    { "271-c4d.c4",    0x00800000, 0x0d457699, ROMType::SPRITE, 0 },
    { "271-c5d.c5",    0x00800000, 0x8a91aae4, ROMType::SPRITE, 0 },
    { "271-c6d.c6",    0x00800000, 0x9f8674b8, ROMType::SPRITE, 0 },
    { "271-c7d.c7",    0x00800000, 0x8ee6b43c, ROMType::SPRITE, 0 },
    { "271-c8d.c8",    0x00800000, 0x6d8d2d60, ROMType::SPRITE, 0 },
    { "271-m1d.m1",    0x00080000, 0xcc8b54c0, ROMType::SOUND_PROGRAM, 0 },
    { "271-v1d.v1",    0x00800000, 0xdd6c6a85, ROMType::SOUND_SAMPLE, 0 },
    { "271-v2d.v2",    0x00800000, 0x0e84f8c1, ROMType::SOUND_SAMPLE, 0 },
};

// The King of Fighters 2003 (NGM-2710, Export)
static const ROMEntry kof2003_roms[] = {
    { "271-p1c.p1",    0x00400000, 0x530ecc14, ROMType::PROGRAM, 0 },
    { "271-p2c.p2",    0x00400000, 0xfd568da9, ROMType::PROGRAM, 0 },
    { "271-p3c.p3",    0x00100000, 0xaec5b4a9, ROMType::PROGRAM, 0 },
    { "271-c1c.c1",    0x00800000, 0xb1dc25d0, ROMType::SPRITE, 0 },
    { "271-c2c.c2",    0x00800000, 0xd5362437, ROMType::SPRITE, 0 },
    { "271-c3c.c3",    0x00800000, 0x0a1fbeab, ROMType::SPRITE, 0 },
    { "271-c4c.c4",    0x00800000, 0x87b19a0c, ROMType::SPRITE, 0 },
    { "271-c5c.c5",    0x00800000, 0x704ea371, ROMType::SPRITE, 0 },
    { "271-c6c.c6",    0x00800000, 0x20a1164c, ROMType::SPRITE, 0 },
    { "271-c7c.c7",    0x00800000, 0x189aba7f, ROMType::SPRITE, 0 },
    { "271-c8c.c8",    0x00800000, 0x20ec4fdc, ROMType::SPRITE, 0 },
    { "271-m1c.m1",    0x00080000, 0xf5515629, ROMType::SOUND_PROGRAM, 0 },
    { "271-v1c.v1",    0x00800000, 0xffa3f8c7, ROMType::SOUND_SAMPLE, 0 },
    { "271-v2c.v2",    0x00800000, 0x5382c7d1, ROMType::SOUND_SAMPLE, 0 },
};

// The Last Blade / Bakumatsu Roman - Gekka no Kenshi (NGM-2340)
static const ROMEntry lastblad_roms[] = {
    { "234-p1.p1",    0x00100000, 0xe123a5a3, ROMType::PROGRAM, 0 },
    { "234-p2.sp2",   0x00400000, 0x0fdc289e, ROMType::PROGRAM, 0 },
    { "234-s1.s1",    0x00020000, 0x95561412, ROMType::TEXT, 0 },
    { "234-c1.c1",    0x00800000, 0x9f7e2bd3, ROMType::SPRITE, 0 },
    { "234-c2.c2",    0x00800000, 0x80623d3c, ROMType::SPRITE, 0 },
    { "234-c3.c3",    0x00800000, 0x91ab1a30, ROMType::SPRITE, 0 },
    { "234-c4.c4",    0x00800000, 0x3d60b037, ROMType::SPRITE, 0 },
    { "234-c5.c5",    0x00400000, 0x1ba80cee, ROMType::SPRITE, 0 },
    { "234-c6.c6",    0x00400000, 0xbeafd091, ROMType::SPRITE, 0 },
    { "234-m1.m1",    0x00020000, 0x087628ea, ROMType::SOUND_PROGRAM, 0 },
    { "234-v1.v1",    0x00400000, 0xed66b76f, ROMType::SOUND_SAMPLE, 0 },
    { "234-v2.v2",    0x00400000, 0xa0e7f6e2, ROMType::SOUND_SAMPLE, 0 },
    { "234-v3.v3",    0x00400000, 0xa506e1e2, ROMType::SOUND_SAMPLE, 0 },
    { "234-v4.v4",    0x00400000, 0x0e34157f, ROMType::SOUND_SAMPLE, 0 },
};

// The Last Blade 2 / Bakumatsu Roman - Dai Ni Maku Gekka no Kenshi (NGM-2430 ~ NGH-2430)
static const ROMEntry lastbld2_roms[] = {
    { "243-pg1.p1",   0x00100000, 0xaf1e6554, ROMType::PROGRAM, 0 },
    { "243-pg2.sp2",  0x00400000, 0xadd4a30b, ROMType::PROGRAM, 0 },
    { "243-s1.s1",    0x00020000, 0xc9cd2298, ROMType::TEXT, 0 },
    { "243-c1.c1",    0x00800000, 0x5839444d, ROMType::SPRITE, 0 },
    { "243-c2.c2",    0x00800000, 0xdd087428, ROMType::SPRITE, 0 },
    { "243-c3.c3",    0x00800000, 0x6054cbe0, ROMType::SPRITE, 0 },
    { "243-c4.c4",    0x00800000, 0x8bd2a9d2, ROMType::SPRITE, 0 },
    { "243-c5.c5",    0x00800000, 0x6a503dcf, ROMType::SPRITE, 0 },
    { "243-c6.c6",    0x00800000, 0xec9c36d0, ROMType::SPRITE, 0 },
    { "243-m1.m1",    0x00020000, 0xacf12d10, ROMType::SOUND_PROGRAM, 0 },
    { "243-v1.v1",    0x00400000, 0xf7ee6fbb, ROMType::SOUND_SAMPLE, 0 },
    { "243-v2.v2",    0x00400000, 0xaa9e4df6, ROMType::SOUND_SAMPLE, 0 },
    { "243-v3.v3",    0x00400000, 0x4ac750b2, ROMType::SOUND_SAMPLE, 0 },
    { "243-v4.v4",    0x00400000, 0xf5c64ba6, ROMType::SOUND_SAMPLE, 0 },
};

// The Super Spy (NGM-011 ~ NGH-011)
static const ROMEntry superspy_roms[] = {
    { "011-p1.p1",    0x00080000, 0xc7f944b5, ROMType::PROGRAM, 0 },
    { "sp2.p2",       0x00020000, 0x811a4faf, ROMType::PROGRAM, 0 },
    { "011-s1.s1",    0x00020000, 0xec5fdb96, ROMType::TEXT, 0 },
    { "011-c1.c1",    0x00100000, 0xcae7be57, ROMType::SPRITE, 0 },
    { "011-c2.c2",    0x00100000, 0x9e29d986, ROMType::SPRITE, 0 },
    { "011-c3.c3",    0x00100000, 0x14832ff2, ROMType::SPRITE, 0 },
    { "011-c4.c4",    0x00100000, 0xb7f63162, ROMType::SPRITE, 0 },
    { "011-m1.m1",    0x00040000, 0xca661f1b, ROMType::SOUND_PROGRAM, 0 },
    { "011-v11.v11",  0x00100000, 0x5c674d5c, ROMType::SOUND_SAMPLE, 0 },
    { "011-v12.v12",  0x00080000, 0x9f513d5a, ROMType::SOUND_SAMPLE, 0 },
    { "011-v21.v21",  0x00080000, 0x426cd040, ROMType::SOUND_SAMPLE, 0 },
};

// The Ultimate 11 - The SNK Football Championship / Tokuten Ou - Honoo no Libero
static const ROMEntry ssideki4_roms[] = {
    { "215-p1.p1",    0x00200000, 0x519b4ba3, ROMType::PROGRAM, 0 },
    { "215-s1.s1",    0x00020000, 0xf0fe5c36, ROMType::TEXT, 0 },
    { "215-c1.c1",    0x00400000, 0x8ff444f5, ROMType::SPRITE, 0 },
    { "215-c2.c2",    0x00400000, 0x5b155037, ROMType::SPRITE, 0 },
    { "215-c3.c3",    0x00400000, 0x456a073a, ROMType::SPRITE, 0 },
    { "215-c4.c4",    0x00400000, 0x43c182e1, ROMType::SPRITE, 0 },
    { "215-c5.c5",    0x00200000, 0x0c6f97ec, ROMType::SPRITE, 0 },
    { "215-c6.c6",    0x00200000, 0x329c5e1b, ROMType::SPRITE, 0 },
    { "215-m1.m1",    0x00020000, 0xa932081d, ROMType::SOUND_PROGRAM, 0 },
    { "215-v1.v1",    0x00400000, 0x877d1409, ROMType::SOUND_SAMPLE, 0 },
    { "215-v2.v2",    0x00200000, 0x1bfa218b, ROMType::SOUND_SAMPLE, 0 },
};

// Thrash Rally (ALM-003 ~ ALH-003)
static const ROMEntry trally_roms[] = {
    { "038-p1.p1",    0x00080000, 0x1e52a576, ROMType::PROGRAM, 0 },
    { "038-p2.p2",    0x00080000, 0xa5193e2f, ROMType::PROGRAM, 0 },
    { "038-s1.s1",    0x00020000, 0xfff62ae3, ROMType::TEXT, 0 },
    { "038-c1.c1",    0x00100000, 0xc58323d4, ROMType::SPRITE, 0 },
    { "038-c2.c2",    0x00100000, 0xbba9c29e, ROMType::SPRITE, 0 },
    { "038-c3.c3",    0x00080000, 0x3bb7b9d6, ROMType::SPRITE, 0 },
    { "038-c4.c4",    0x00080000, 0xa4513ecf, ROMType::SPRITE, 0 },
    { "038-m1.m1",    0x00020000, 0x0908707e, ROMType::SOUND_PROGRAM, 0 },
    { "038-v1.v1",    0x00100000, 0x5ccd9fd5, ROMType::SOUND_SAMPLE, 0 },
    { "038-v2.v2",    0x00080000, 0xddd8d1e6, ROMType::SOUND_SAMPLE, 0 },
};

// Top Hunter - Roddy & Cathy (NGM-046)
static const ROMEntry tophuntr_roms[] = {
    { "046-p1.p1",    0x00100000, 0x69fa9e29, ROMType::PROGRAM, 0 },
    { "046-p2.sp2",   0x00100000, 0xf182cb3e, ROMType::PROGRAM, 0 },
    { "046-s1.s1",    0x00020000, 0x14b01d7b, ROMType::TEXT, 0 },
    { "046-c1.c1",    0x00100000, 0xfa720a4a, ROMType::SPRITE, 0 },
    { "046-c2.c2",    0x00100000, 0xc900c205, ROMType::SPRITE, 0 },
    { "046-c3.c3",    0x00100000, 0x880e3c25, ROMType::SPRITE, 0 },
    { "046-c4.c4",    0x00100000, 0x7a2248aa, ROMType::SPRITE, 0 },
    { "046-c5.c5",    0x00100000, 0x4b735e45, ROMType::SPRITE, 0 },
    { "046-c6.c6",    0x00100000, 0x273171df, ROMType::SPRITE, 0 },
    { "046-c7.c7",    0x00100000, 0x12829c4c, ROMType::SPRITE, 0 },
    { "046-c8.c8",    0x00100000, 0xc944e03d, ROMType::SPRITE, 0 },
    { "046-m1.m1",    0x00020000, 0x3f84bb9f, ROMType::SOUND_PROGRAM, 0 },
    { "046-v1.v1",    0x00100000, 0xc1f9c2db, ROMType::SOUND_SAMPLE, 0 },
    { "046-v2.v2",    0x00100000, 0x56254a64, ROMType::SOUND_SAMPLE, 0 },
    { "046-v3.v3",    0x00100000, 0x58113fb1, ROMType::SOUND_SAMPLE, 0 },
    { "046-v4.v4",    0x00100000, 0x4f54c187, ROMType::SOUND_SAMPLE, 0 },
};

// Top Player's Golf (NGM-003 ~ NGH-003)
static const ROMEntry tpgolf_roms[] = {
    { "003-p1.p1",    0x00080000, 0xf75549ba, ROMType::PROGRAM, 0 },
    { "003-p2.p2",    0x00080000, 0xb7809a8f, ROMType::PROGRAM, 0 },
    { "003-s1.s1",    0x00020000, 0x7b3eb9b1, ROMType::TEXT, 0 },
    { "003-c1.c1",    0x00080000, 0x0315fbaf, ROMType::SPRITE, 0 },
    { "003-c2.c2",    0x00080000, 0xb4c15d59, ROMType::SPRITE, 0 },
    { "003-c3.c3",    0x00080000, 0x8ce3e8da, ROMType::SPRITE, 0 },
    { "003-c4.c4",    0x00080000, 0x29725969, ROMType::SPRITE, 0 },
    { "003-c5.c5",    0x00080000, 0x9a7146da, ROMType::SPRITE, 0 },
    { "003-c6.c6",    0x00080000, 0x1e63411a, ROMType::SPRITE, 0 },
    { "003-c7.c7",    0x00080000, 0x2886710c, ROMType::SPRITE, 0 },
    { "003-c8.c8",    0x00080000, 0x422af22d, ROMType::SPRITE, 0 },
    { "003-m1.m1",    0x00020000, 0x4cc545e6, ROMType::SOUND_PROGRAM, 0 },
    { "003-v11.v11",  0x00080000, 0xff97f1cb, ROMType::SOUND_SAMPLE, 0 },
    { "003-v21.v21",  0x00080000, 0xd34960c6, ROMType::SOUND_SAMPLE, 0 },
    { "003-v22.v22",  0x00080000, 0x9a5f58d4, ROMType::SOUND_SAMPLE, 0 },
    { "003-v23.v23",  0x00080000, 0x30f53e54, ROMType::SOUND_SAMPLE, 0 },
    { "003-v24.v24",  0x00080000, 0x5ba0f501, ROMType::SOUND_SAMPLE, 0 },
};

// Treasure of the Caribbean
static const ROMEntry totc_roms[] = {
    { "316-p1.p1",    0x00100000, 0x99604539, ROMType::PROGRAM, 0 },
    { "316-s1.s1",    0x00020000, 0x0a3fee41, ROMType::TEXT, 0 },
    { "316-c1.c1",    0x00200000, 0xcdd6600f, ROMType::SPRITE, 0 },
    { "316-c1.c2",    0x00200000, 0xf362c271, ROMType::SPRITE, 0 },
    { "316-m1.m1",    0x00020000, 0x18b23ace, ROMType::SOUND_PROGRAM, 0 },
    { "316-v1.v1",    0x00200000, 0x15c7f9e6, ROMType::SOUND_SAMPLE, 0 },
    { "316-v2.v2",    0x00200000, 0x1b264559, ROMType::SOUND_SAMPLE, 0 },
    { "316-v3.v3",    0x00100000, 0x84b62c5d, ROMType::SOUND_SAMPLE, 0 },
};

// Twinkle Star Sprites
static const ROMEntry twinspri_roms[] = {
    { "224-p1.p1",    0x00200000, 0x7697e445, ROMType::PROGRAM, 0 },
    { "224-s1.s1",    0x00020000, 0xeeed5758, ROMType::TEXT, 0 },
    { "224-c1.c1",    0x00400000, 0xf7da64ab, ROMType::SPRITE, 0 },
    { "224-c2.c2",    0x00400000, 0x4c09bbfb, ROMType::SPRITE, 0 },
    { "224-c3.c3",    0x00100000, 0xc59e4129, ROMType::SPRITE, 0 },
    { "224-c4.c4",    0x00100000, 0xb5532e53, ROMType::SPRITE, 0 },
    { "224-m1.m1",    0x00020000, 0x364d6f96, ROMType::SOUND_PROGRAM, 0 },
    { "224-v1.v1",    0x00400000, 0xff57f088, ROMType::SOUND_SAMPLE, 0 },
    { "224-v2.v2",    0x00200000, 0x7ad26599, ROMType::SOUND_SAMPLE, 0 },
};

// V-Liner (v0.7a)
static const ROMEntry vliner_roms[] = {
    { "epr_7a.p1",    0x00080000, 0x052f93ed, ROMType::PROGRAM, 0 },
    { "s-1.s1",       0x00020000, 0x972d8c31, ROMType::TEXT, 0 },
    { "c-1.c1",       0x00080000, 0x5118f7c0, ROMType::SPRITE, 0 },
    { "c-2.c2",       0x00080000, 0xefe9b33e, ROMType::SPRITE, 0 },
    { "m-1.m1",       0x00010000, 0x9b92b7d1, ROMType::SOUND_PROGRAM, 0 },
};

// Viewpoint
static const ROMEntry viewpoin_roms[] = {
    { "051-p1.p1",    0x00100000, 0x17aa899d, ROMType::PROGRAM, 0 },
    { "051-s1.s1",    0x00020000, 0x9fea5758, ROMType::TEXT, 0 },
    { "051-c1.c1",    0x00200000, 0xd624c132, ROMType::SPRITE, 0 },
    { "051-c2.c2",    0x00200000, 0x40d69f1e, ROMType::SPRITE, 0 },
    { "051-m1.m1",    0x00020000, 0x8e69f29a, ROMType::SOUND_PROGRAM, 0 },
    { "051-v2.v1",    0x00200000, 0x019978b6, ROMType::SOUND_SAMPLE, 0 },
    { "051-v4.v2",    0x00200000, 0x5758f38c, ROMType::SOUND_SAMPLE, 0 },
};

// Voltage Fighter - Gowcaizer / Choujin Gakuen Gowcaizer
static const ROMEntry gowcaizr_roms[] = {
    { "094-p1.p1",    0x00200000, 0x33019545, ROMType::PROGRAM, 0 },
    { "094-s1.s1",    0x00020000, 0x2f8748a2, ROMType::TEXT, 0 },
    { "094-c1.c1",    0x00200000, 0x042f6af5, ROMType::SPRITE, 0 },
    { "094-c2.c2",    0x00200000, 0x0fbcd046, ROMType::SPRITE, 0 },
    { "094-c3.c3",    0x00200000, 0x58bfbaa1, ROMType::SPRITE, 0 },
    { "094-c4.c4",    0x00200000, 0x9451ee73, ROMType::SPRITE, 0 },
    { "094-c5.c5",    0x00200000, 0xff9cf48c, ROMType::SPRITE, 0 },
    { "094-c6.c6",    0x00200000, 0x31bbd918, ROMType::SPRITE, 0 },
    { "094-c7.c7",    0x00200000, 0x2091ec04, ROMType::SPRITE, 0 },
    { "094-c8.c8",    0x00200000, 0xd80dd241, ROMType::SPRITE, 0 },
    { "094-m1.m1",    0x00020000, 0x78c851cb, ROMType::SOUND_PROGRAM, 0 },
    { "094-v1.v1",    0x00200000, 0x6c31223c, ROMType::SOUND_SAMPLE, 0 },
    { "094-v2.v2",    0x00200000, 0x8edb776c, ROMType::SOUND_SAMPLE, 0 },
    { "094-v3.v3",    0x00100000, 0xc63b9285, ROMType::SOUND_SAMPLE, 0 },
};

// Waku Waku 7
static const ROMEntry wakuwak7_roms[] = {
    { "225-p1.p1",    0x00100000, 0xb14da766, ROMType::PROGRAM, 0 },
    { "225-p2.sp2",   0x00200000, 0xfe190665, ROMType::PROGRAM, 0 },
    { "225-s1.s1",    0x00020000, 0x71c4b4b5, ROMType::TEXT, 0 },
    { "225-c1.c1",    0x00400000, 0xee4fea54, ROMType::SPRITE, 0 },
    { "225-c2.c2",    0x00400000, 0x0c549e2d, ROMType::SPRITE, 0 },
    { "225-c3.c3",    0x00400000, 0xaf0897c0, ROMType::SPRITE, 0 },
    { "225-c4.c4",    0x00400000, 0x4c66527a, ROMType::SPRITE, 0 },
    { "225-c5.c5",    0x00400000, 0x8ecea2b5, ROMType::SPRITE, 0 },
    { "225-c6.c6",    0x00400000, 0x0eb11a6d, ROMType::SPRITE, 0 },
    { "225-m1.m1",    0x00020000, 0x0634bba6, ROMType::SOUND_PROGRAM, 0 },
    { "225-v1.v1",    0x00400000, 0x6195c6b4, ROMType::SOUND_SAMPLE, 0 },
    { "225-v2.v2",    0x00400000, 0x6159c5fe, ROMType::SOUND_SAMPLE, 0 },
};

// Windjammers / Flying Power Disc
static const ROMEntry wjammers_roms[] = {
    { "065-p1.p1",    0x00100000, 0x6692c140, ROMType::PROGRAM, 0 },
    { "065-s1.s1",    0x00020000, 0x074b5723, ROMType::TEXT, 0 },
    { "065-c1.c1",    0x00100000, 0xc7650204, ROMType::SPRITE, 0 },
    { "065-c2.c2",    0x00100000, 0xd9f3e71d, ROMType::SPRITE, 0 },
    { "065-c3.c3",    0x00100000, 0x40986386, ROMType::SPRITE, 0 },
    { "065-c4.c4",    0x00100000, 0x715e15ff, ROMType::SPRITE, 0 },
    { "065-m1.m1",    0x00020000, 0x52c23cfc, ROMType::SOUND_PROGRAM, 0 },
    { "065-v1.v1",    0x00100000, 0xce8b3698, ROMType::SOUND_SAMPLE, 0 },
    { "065-v2.v2",    0x00100000, 0x659f9b96, ROMType::SOUND_SAMPLE, 0 },
    { "065-v3.v3",    0x00100000, 0x39f73061, ROMType::SOUND_SAMPLE, 0 },
    { "065-v4.v4",    0x00100000, 0x5dee7963, ROMType::SOUND_SAMPLE, 0 },
};

// World Heroes (ALM-005)
static const ROMEntry wh1_roms[] = {
    { "053-epr.p1",   0x00080000, 0xd42e1e9a, ROMType::PROGRAM, 0 },
    { "053-epr.p2",   0x00080000, 0x0e33e8a3, ROMType::PROGRAM, 0 },
    { "053-s1.s1",    0x00020000, 0x8c2c2d6b, ROMType::TEXT, 0 },
    { "053-c1.c1",    0x00200000, 0x85eb5bce, ROMType::SPRITE, 0 },
    { "053-c2.c2",    0x00200000, 0xec93b048, ROMType::SPRITE, 0 },
    { "053-c3.c3",    0x00100000, 0x0dd64965, ROMType::SPRITE, 0 },
    { "053-c4.c4",    0x00100000, 0x9270d954, ROMType::SPRITE, 0 },
    { "053-m1.m1",    0x00020000, 0x1bd9d04b, ROMType::SOUND_PROGRAM, 0 },
    { "053-v2.v2",    0x00200000, 0xa68df485, ROMType::SOUND_SAMPLE, 0 },
    { "053-v4.v4",    0x00100000, 0x7bea8f66, ROMType::SOUND_SAMPLE, 0 },
};

// World Heroes 2 (ALM-006 ~ ALH-006)
static const ROMEntry wh2_roms[] = {
    { "057-p1.p1",    0x00200000, 0x65a891d9, ROMType::PROGRAM, 0 },
    { "057-s1.s1",    0x00020000, 0xfcaeb3a4, ROMType::TEXT, 0 },
    { "057-c1.c1",    0x00200000, 0x21c6bb91, ROMType::SPRITE, 0 },
    { "057-c2.c2",    0x00200000, 0xa3999925, ROMType::SPRITE, 0 },
    { "057-c3.c3",    0x00200000, 0xb725a219, ROMType::SPRITE, 0 },
    { "057-c4.c4",    0x00200000, 0x8d96425e, ROMType::SPRITE, 0 },
    { "057-c5.c5",    0x00200000, 0xb20354af, ROMType::SPRITE, 0 },
    { "057-c6.c6",    0x00200000, 0xb13d1de3, ROMType::SPRITE, 0 },
    { "057-m1.m1",    0x00020000, 0x8fa3bc77, ROMType::SOUND_PROGRAM, 0 },
    { "057-v1.v1",    0x00200000, 0x8877e301, ROMType::SOUND_SAMPLE, 0 },
    { "057-v2.v2",    0x00200000, 0xc1317ff4, ROMType::SOUND_SAMPLE, 0 },
};

// World Heroes 2 Jet (ADM-007 ~ ADH-007)
static const ROMEntry wh2j_roms[] = {
    { "064-p1.p1",    0x00200000, 0x385a2e86, ROMType::PROGRAM, 0 },
    { "064-s1.s1",    0x00020000, 0x2a03998a, ROMType::TEXT, 0 },
    { "064-c1.c1",    0x00200000, 0x2ec87cea, ROMType::SPRITE, 0 },
    { "064-c2.c2",    0x00200000, 0x526b81ab, ROMType::SPRITE, 0 },
    { "064-c3.c3",    0x00200000, 0x436d1b31, ROMType::SPRITE, 0 },
    { "064-c4.c4",    0x00200000, 0xf9c8dd26, ROMType::SPRITE, 0 },
    { "064-c5.c5",    0x00200000, 0x8e34a9f4, ROMType::SPRITE, 0 },
    { "064-c6.c6",    0x00200000, 0xa43e4766, ROMType::SPRITE, 0 },
    { "064-c7.c7",    0x00200000, 0x59d97215, ROMType::SPRITE, 0 },
    { "064-c8.c8",    0x00200000, 0xfc092367, ROMType::SPRITE, 0 },
    { "064-m1.m1",    0x00020000, 0xd2eec9d3, ROMType::SOUND_PROGRAM, 0 },
    { "064-v1.v1",    0x00200000, 0xaa277109, ROMType::SOUND_SAMPLE, 0 },
    { "064-v2.v2",    0x00200000, 0xb6527edd, ROMType::SOUND_SAMPLE, 0 },
};

// World Heroes Perfect
static const ROMEntry whp_roms[] = {
    { "090-p1.p1",    0x00200000, 0xafaa4702, ROMType::PROGRAM, 0 },
    { "090-s1.s1",    0x00020000, 0x174a880f, ROMType::TEXT, 0 },
    { "090-c1.c1",    0x00400000, 0xcd30ed9b, ROMType::SPRITE, 0 },
    { "090-c2.c2",    0x00400000, 0x10eed5ee, ROMType::SPRITE, 0 },
    { "064-c3.c3",    0x00200000, 0x436d1b31, ROMType::SPRITE, 0 },
    { "064-c4.c4",    0x00200000, 0xf9c8dd26, ROMType::SPRITE, 0 },
    { "064-c5.c5",    0x00200000, 0x8e34a9f4, ROMType::SPRITE, 0 },
    { "064-c6.c6",    0x00200000, 0xa43e4766, ROMType::SPRITE, 0 },
    { "064-c7.c7",    0x00200000, 0x59d97215, ROMType::SPRITE, 0 },
    { "064-c8.c8",    0x00200000, 0xfc092367, ROMType::SPRITE, 0 },
    { "090-m1.m1",    0x00020000, 0x28065668, ROMType::SOUND_PROGRAM, 0 },
    { "090-v1.v1",    0x00200000, 0x30cf2709, ROMType::SOUND_SAMPLE, 0 },
    { "064-v2.v2",    0x00200000, 0xb6527edd, ROMType::SOUND_SAMPLE, 0 },
    { "090-v3.v3",    0x00200000, 0x1908a7ce, ROMType::SOUND_SAMPLE, 0 },
};

// Zed Blade / Operation Ragnarok
static const ROMEntry zedblade_roms[] = {
    { "076-p1.p1",    0x00080000, 0xd7c1effd, ROMType::PROGRAM, 0 },
    { "076-s1.s1",    0x00020000, 0xf4c25dd5, ROMType::TEXT, 0 },
    { "076-c1.c1",    0x00200000, 0x4d9cb038, ROMType::SPRITE, 0 },
    { "076-c2.c2",    0x00200000, 0x09233884, ROMType::SPRITE, 0 },
    { "076-c3.c3",    0x00200000, 0xd06431e3, ROMType::SPRITE, 0 },
    { "076-c4.c4",    0x00200000, 0x4b1c089b, ROMType::SPRITE, 0 },
    { "076-m1.m1",    0x00020000, 0x7b5f3d0a, ROMType::SOUND_PROGRAM, 0 },
    { "076-v1.v1",    0x00200000, 0x1a21d90c, ROMType::SOUND_SAMPLE, 0 },
    { "076-v2.v2",    0x00200000, 0xb61686c3, ROMType::SOUND_SAMPLE, 0 },
    { "076-v3.v3",    0x00100000, 0xb90658fa, ROMType::SOUND_SAMPLE, 0 },
};

// Zintrick / Oshidashi Zentrix (bootleg of CD version)
static const ROMEntry zintrckb_roms[] = {
    { "zin-p1.bin",    0x00100000, 0x06c8fca7, ROMType::PROGRAM, 0 },
    { "zin-s1.bin",    0x00020000, 0xa7ab0e81, ROMType::TEXT, 0 },
    { "zin-c1.bin",    0x00200000, 0x76aee189, ROMType::SPRITE, 0 },
    { "zin-c2.bin",    0x00200000, 0x844ed4b3, ROMType::SPRITE, 0 },
    { "zin-m1.bin",    0x00020000, 0xfd9627ca, ROMType::SOUND_PROGRAM, 0 },
    { "zin-v1.bin",    0x00200000, 0xc09f74f1, ROMType::SOUND_SAMPLE, 0 },
};

// Zupapa!
static const ROMEntry zupapa_roms[] = {
    { "070-p1.p1",    0x00100000, 0x5a96203e, ROMType::PROGRAM, 0 },
    { "070-c1.c1",    0x00800000, 0xf8ad02d8, ROMType::SPRITE, 0 },
    { "070-c2.c2",    0x00800000, 0x70156dde, ROMType::SPRITE, 0 },
    { "070-epr.m1",   0x00020000, 0x5a3b3191, ROMType::SOUND_PROGRAM, 0 },
    { "070-v1.v1",    0x00200000, 0xd3a7e1ff, ROMType::SOUND_SAMPLE, 0 },
};

// Game database
const GameInfo GameDatabase::s_games[] = {
    {
        "19yy", "19YY (Neo CD conversion, ADK World)", _19yy_roms, sizeof(_19yy_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_SWAPP
    },
    {
        "2020bb", "2020 Super Baseball (set 1)", _2020bb_roms, sizeof(_2020bb_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "3countb", "3 Count Bout / Fire Suplex (NGM-043 ~ NGH-043)", _3countb_roms, sizeof(_3countb_roms) / sizeof(ROMEntry),
        320, 224, GAME_FLAG_SWAPC
    },
    {
        "sonicwi2", "Aero Fighters 2 / Sonic Wings 2", sonicwi2_roms, sizeof(sonicwi2_roms) / sizeof(ROMEntry),
        320, 224, GAME_FLAG_SWAPP
    },
    {
        "sonicwi3", "Aero Fighters 3 / Sonic Wings 3", sonicwi3_roms, sizeof(sonicwi3_roms) / sizeof(ROMEntry),
        320, 224, GAME_FLAG_SWAPP
    },
    {
        "aodk", "Aggressors of Dark Kombat / Tsuukai GANGAN Koushinkyoku (ADM-008 ~ ADH-008)", aodk_roms, sizeof(aodk_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_SWAPP
    },
    {
        "alpham2", "Alpha Mission II / ASO II - Last Guardian (NGM-007 ~ NGH-007)", alpham2_roms, sizeof(alpham2_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "androdun", "Andro Dunos (NGM-049 ~ NGH-049)", androdun_roms, sizeof(androdun_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "aof", "Art of Fighting / Ryuuko no Ken (NGM-044 ~ NGH-044)", aof_roms, sizeof(aof_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_SWAPC
    },
    {
        "aof2", "Art of Fighting 2 / Ryuuko no Ken 2 (NGM-056)", aof2_roms, sizeof(aof2_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "aof3", "Art of Fighting 3 - The Path of the Warrior / Art of Fighting - Ryuuko no Ken Gaiden", aof3_roms, sizeof(aof3_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "bakatono", "Bakatonosama Mahjong Manyuuki (MOM-002 ~ MOH-002)", bakatono_roms, sizeof(bakatono_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "b2b", "Bang Bang Busters (2010 NCI release)", b2b_roms, sizeof(b2b_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "bangbead", "Bang Bead", bangbead_roms, sizeof(bangbead_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_SWAPP | GAME_FLAG_CMC42
    },
    {
        "bstars2", "Baseball Stars 2", bstars2_roms, sizeof(bstars2_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "bstars", "Baseball Stars Professional (NGM-002)", bstars_roms, sizeof(bstars_roms) / sizeof(ROMEntry),
        320, 224, 0
    },
    {
        "flipshot", "Battle Flip Shot", flipshot_roms, sizeof(flipshot_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "blazstar", "Blazing Star", blazstar_roms, sizeof(blazstar_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "bjourney", "Blue's Journey / Raguy (ALM-001 ~ ALH-001)", bjourney_roms, sizeof(bjourney_roms) / sizeof(ROMEntry),
        320, 224, 0
    },
    {
        "breakrev", "Breakers Revenge", breakrev_roms, sizeof(breakrev_roms) / sizeof(ROMEntry),
        320, 224, GAME_FLAG_SWAPP
    },
    {
        "breakers", "Breakers", breakers_roms, sizeof(breakers_roms) / sizeof(ROMEntry),
        320, 224, GAME_FLAG_SWAPP
    },
    {
        "burningf", "Burning Fight (NGM-018 ~ NGH-018)", burningf_roms, sizeof(burningf_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "ctomaday", "Captain Tomaday", ctomaday_roms, sizeof(ctomaday_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_SWAPP
    },
    {
        "marukodq", "Chibi Maruko-chan: Maruko Deluxe Quiz", marukodq_roms, sizeof(marukodq_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "ironclad", "Choutetsu Brikin'ger / Iron Clad (prototype)", ironclad_roms, sizeof(ironclad_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_SWAPP
    },
    {
        "crsword", "Crossed Swords (ALM-002 ~ ALH-002)", crsword_roms, sizeof(crsword_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "crswd2bl", "Crossed Swords 2 (bootleg of CD version)", crswd2bl_roms, sizeof(crswd2bl_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_SWAPP
    },
    {
        "cyberlip", "Cyber-Lip (NGM-010)", cyberlip_roms, sizeof(cyberlip_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "doubledr", "Double Dragon (Neo-Geo)", doubledr_roms, sizeof(doubledr_roms) / sizeof(ROMEntry),
        320, 224, GAME_FLAG_SWAPP
    },
    {
        "eightman", "Eight Man (NGM-025 ~ NGH-025)", eightman_roms, sizeof(eightman_roms) / sizeof(ROMEntry),
        320, 224, 0
    },
    {
        "kabukikl", "Far East of Eden - Kabuki Klash / Tengai Makyou - Shin Den", kabukikl_roms, sizeof(kabukikl_roms) / sizeof(ROMEntry),
        320, 224, GAME_FLAG_SWAPP
    },
    {
        "fatfury1", "Fatal Fury - King of Fighters / Garou Densetsu - Shukumei no Tatakai (NGM-033 ~ NGH-033)", fatfury1_roms, sizeof(fatfury1_roms) / sizeof(ROMEntry),
        320, 224, 0
    },
    {
        "fatfury2", "Fatal Fury 2 / Garou Densetsu 2 - Arata-naru Tatakai (NGM-047 ~ NGH-047)", fatfury2_roms, sizeof(fatfury2_roms) / sizeof(ROMEntry),
        320, 224, GAME_FLAG_SWAPC
    },
    {
        "fatfury3", "Fatal Fury 3 - Road to the Final Victory / Garou Densetsu 3 - Haruka-naru Tatakai (NGM-069 ~ NGH-069)", fatfury3_roms, sizeof(fatfury3_roms) / sizeof(ROMEntry),
        320, 224, 0
    },
    {
        "fatfursp", "Fatal Fury Special / Garou Densetsu Special (NGM-058 ~ NGH-058, set 1)", fatfursp_roms, sizeof(fatfursp_roms) / sizeof(ROMEntry),
        320, 224, 0
    },
    {
        "fightfev", "Fight Fever / Wang Jung Wang (set 1)", fightfev_roms, sizeof(fightfev_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "fbfrenzy", "Football Frenzy (NGM-034 ~ NGH-034)", fbfrenzy_roms, sizeof(fbfrenzy_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "galaxyfg", "Galaxy Fight - Universal Warriors", galaxyfg_roms, sizeof(galaxyfg_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_SWAPP
    },
    {
        "ganryu", "Ganryu / Musashi Ganryuki", ganryu_roms, sizeof(ganryu_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_SWAPP | GAME_FLAG_CMC42
    },
    {
        "garou", "Garou - Mark of the Wolves (NGM-2530)", garou_roms, sizeof(garou_roms) / sizeof(ROMEntry),
        320, 224, GAME_FLAG_CMC42 | GAME_FLAG_SMA_PROTECTION
    },
    {
        "gpilots", "Ghost Pilots (NGM-020 ~ NGH-020)", gpilots_roms, sizeof(gpilots_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "ghostlop", "Ghostlop (prototype)", ghostlop_roms, sizeof(ghostlop_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "goalx3", "Goal! Goal! Goal!", goalx3_roms, sizeof(goalx3_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_SWAPP
    },
    {
        "gururin", "Gururin", gururin_roms, sizeof(gururin_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "froman2b", "Idol Mahjong Final Romance 2 (Neo-Geo, bootleg of CD version)", froman2b_roms, sizeof(froman2b_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "janshin", "Janshin Densetsu - Quest of Jongmaster", janshin_roms, sizeof(janshin_roms) / sizeof(ROMEntry),
        320, 224, 0
    },
    {
        "jockeygp", "Jockey Grand Prix (set 1)", jockeygp_roms, sizeof(jockeygp_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_CMC50 | GAME_FLAG_ENCRYPTED_M1
    },
    {
        "karnovr", "Karnov's Revenge / Fighter's History Dynamite", karnovr_roms, sizeof(karnovr_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "kotm", "King of the Monsters (set 1)", kotm_roms, sizeof(kotm_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "kotm2", "King of the Monsters 2 - The Next Thing (NGM-039 ~ NGH-039)", kotm2_roms, sizeof(kotm2_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_SWAPC
    },
    {
        "kizuna", "Kizuna Encounter - Super Tag Battle / Fu'un Super Tag Battle", kizuna_roms, sizeof(kizuna_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_SWAPP
    },
    {
        "lasthope", "Last Hope (bootleg AES to MVS conversion, no coin support)", lasthope_roms, sizeof(lasthope_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "lresort", "Last Resort", lresort_roms, sizeof(lresort_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "lbowling", "League Bowling (NGM-019 ~ NGH-019)", lbowling_roms, sizeof(lbowling_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "legendos", "Legend of Success Joe / Ashita no Joe Densetsu", legendos_roms, sizeof(legendos_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "magdrop3", "Magical Drop III", magdrop3_roms, sizeof(magdrop3_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "magdrop2", "Magical Drop II", magdrop2_roms, sizeof(magdrop2_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "maglord", "Magician Lord (NGM-005)", maglord_roms, sizeof(maglord_roms) / sizeof(ROMEntry),
        320, 224, 0
    },
    {
        "mahretsu", "Mahjong Kyo Retsuden (NGM-004 ~ NGH-004)", mahretsu_roms, sizeof(mahretsu_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "matrim", "Matrimelee / Shin Gouketsuji Ichizoku Toukon (NGM-2660 ~ NGH-2660)", matrim_roms, sizeof(matrim_roms) / sizeof(ROMEntry),
        320, 224, GAME_FLAG_CMC50 | GAME_FLAG_ALTERNATE_TEXT | GAME_FLAG_ENCRYPTED_M1
    },
    {
        "mslug", "Metal Slug - Super Vehicle-001", mslug_roms, sizeof(mslug_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_SWAPP
    },
    {
        "mslug2", "Metal Slug 2 - Super Vehicle-001/II (NGM-2410 ~ NGH-2410)", mslug2_roms, sizeof(mslug2_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "mslug3fd", "Metal Slug 3 (Fully Decrypted)", mslug3fd_roms, sizeof(mslug3fd_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "mslug3", "Metal Slug 3 (NGM-2560)", mslug3_roms, sizeof(mslug3_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_CMC42 | GAME_FLAG_SMA_PROTECTION
    },
    {
        "mslug4fd", "Metal Slug 4 (Fully Decrypted)", mslug4fd_roms, sizeof(mslug4fd_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "mslug4", "Metal Slug 4 (NGM-2630)", mslug4_roms, sizeof(mslug4_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_CMC50 | GAME_FLAG_ENCRYPTED_M1
    },
    {
        "mslug5fd", "Metal Slug 5 (Fully Decrypted)", mslug5fd_roms, sizeof(mslug5fd_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "mslug5", "Metal Slug 5 (NGM-2680)", mslug5_roms, sizeof(mslug5_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_CMC50 | GAME_FLAG_ALTERNATE_TEXT | GAME_FLAG_ENCRYPTED_M1 | GAME_FLAG_P32
    },
    {
        "mslugx", "Metal Slug X - Super Vehicle-001 (NGM-2500 ~ NGH-2500)", mslugx_roms, sizeof(mslugx_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "minasan", "Minasan no Okagesamadesu! Dai Sugoroku Taikai (MOM-001 ~ MOH-001)", minasan_roms, sizeof(minasan_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "miexchng", "Money Puzzle Exchanger / Money Idol Exchanger", miexchng_roms, sizeof(miexchng_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "mutnat", "Mutation Nation (NGM-014 ~ NGH-014)", mutnat_roms, sizeof(mutnat_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "nam1975", "NAM-1975 (NGM-001 ~ NGH-001)", nam1975_roms, sizeof(nam1975_roms) / sizeof(ROMEntry),
        320, 224, 0
    },
    {
        "neobombe", "Neo Bomberman", neobombe_roms, sizeof(neobombe_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "neodrift", "Neo Drift Out - New Technology", neodrift_roms, sizeof(neodrift_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_SWAPP
    },
    {
        "neomrdo", "Neo Mr. Do!", neomrdo_roms, sizeof(neomrdo_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "turfmast", "Neo Turf Masters / Big Tournament Golf", turfmast_roms, sizeof(turfmast_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_SWAPP
    },
    {
        "neocup98", "Neo-Geo Cup '98 - The Road to the Victory", neocup98_roms, sizeof(neocup98_roms) / sizeof(ROMEntry),
        320, 224, GAME_FLAG_SWAPP
    },
    {
        "nitd", "Nightmare in the Dark", nitd_roms, sizeof(nitd_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_CMC42
    },
    {
        "ncombat", "Ninja Combat (NGM-009)", ncombat_roms, sizeof(ncombat_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "ncommand", "Ninja Commando", ncommand_roms, sizeof(ncommand_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "ninjamas", "Ninja Master's - Haoh-ninpo-cho", ninjamas_roms, sizeof(ninjamas_roms) / sizeof(ROMEntry),
        320, 224, 0
    },
    {
        "overtop", "Over Top", overtop_roms, sizeof(overtop_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_SWAPP
    },
    {
        "panicbom", "Panic Bomber", panicbom_roms, sizeof(panicbom_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "pgoal", "Pleasure Goal / Futsal - 5 on 5 Mini Soccer (NGM-219)", pgoal_roms, sizeof(pgoal_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_SWAPP
    },
    {
        "pnyaa", "Pochi and Nyaa (Ver 2.02)", pnyaa_roms, sizeof(pnyaa_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_CMC50 | GAME_FLAG_ENCRYPTED_M1
    },
    {
        "popbounc", "Pop 'n Bounce / Gapporin", popbounc_roms, sizeof(popbounc_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "pspikes2", "Power Spikes II (NGM-068)", pspikes2_roms, sizeof(pspikes2_roms) / sizeof(ROMEntry),
        320, 224, 0
    },
    {
        "preisle2", "Prehistoric Isle 2", preisle2_roms, sizeof(preisle2_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_CMC42
    },
    {
        "pulstar", "Pulstar", pulstar_roms, sizeof(pulstar_roms) / sizeof(ROMEntry),
        320, 224, 0
    },
    {
        "pbobblen", "Puzzle Bobble / Bust-A-Move (Neo-Geo, NGM-083)", pbobblen_roms, sizeof(pbobblen_roms) / sizeof(ROMEntry),
        320, 224, 0
    },
    {
        "pbobbl2n", "Puzzle Bobble 2 / Bust-A-Move Again (Neo-Geo)", pbobbl2n_roms, sizeof(pbobbl2n_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "puzzldpr", "Puzzle De Pon! R!", puzzldpr_roms, sizeof(puzzldpr_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "puzzledp", "Puzzle De Pon!", puzzledp_roms, sizeof(puzzledp_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "joyjoy", "Puzzled / Joy Joy Kid (NGM-021 ~ NGH-021)", joyjoy_roms, sizeof(joyjoy_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "quizdais", "Quiz Daisousa Sen - The Last Count Down (NGM-023 ~ NGH-023)", quizdais_roms, sizeof(quizdais_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "quizkof", "Quiz King of Fighters (SAM-080 ~ SAH-080)", quizkof_roms, sizeof(quizkof_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "quizdai2", "Quiz Meitantei Neo & Geo - Quiz Daisousa Sen part 2 (NGM-042 ~ NGH-042)", quizdai2_roms, sizeof(quizdai2_roms) / sizeof(ROMEntry),
        320, 224, 0
    },
    {
        "rotd", "Rage of the Dragons (NGM-2640?)", rotd_roms, sizeof(rotd_roms) / sizeof(ROMEntry),
        320, 224, GAME_FLAG_CMC50 | GAME_FLAG_ENCRYPTED_M1
    },
    {
        "ragnagrd", "Ragnagard / Shin-Oh-Ken", ragnagrd_roms, sizeof(ragnagrd_roms) / sizeof(ROMEntry),
        320, 224, GAME_FLAG_SWAPP
    },
    {
        "rbff1", "Real Bout Fatal Fury / Real Bout Garou Densetsu (NGM-095 ~ NGH-095)", rbff1_roms, sizeof(rbff1_roms) / sizeof(ROMEntry),
        320, 224, 0
    },
    {
        "rbff2", "Real Bout Fatal Fury 2 - The Newcomers / Real Bout Garou Densetsu 2 - The Newcomers (NGM-2400)", rbff2_roms, sizeof(rbff2_roms) / sizeof(ROMEntry),
        320, 224, 0
    },
    {
        "rbffspec", "Real Bout Fatal Fury Special / Real Bout Garou Densetsu Special", rbffspec_roms, sizeof(rbffspec_roms) / sizeof(ROMEntry),
        320, 224, 0
    },
    {
        "ridhero", "Riding Hero (NGM-006 ~ NGH-006)", ridhero_roms, sizeof(ridhero_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "roboarmy", "Robo Army", roboarmy_roms, sizeof(roboarmy_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "samsho", "Samurai Shodown / Samurai Spirits (NGM-045)", samsho_roms, sizeof(samsho_roms) / sizeof(ROMEntry),
        320, 224, 0
    },
    {
        "samsho2", "Samurai Shodown II / Shin Samurai Spirits - Haohmaru Jigokuhen (NGM-063 ~ NGH-063)", samsho2_roms, sizeof(samsho2_roms) / sizeof(ROMEntry),
        320, 224, GAME_FLAG_SWAPP
    },
    {
        "samsho3", "Samurai Shodown III / Samurai Spirits - Zankurou Musouken (NGM-087)", samsho3_roms, sizeof(samsho3_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "samsho4", "Samurai Shodown IV - Amakusa's Revenge / Samurai Spirits - Amakusa Kourin (NGM-222 ~ NGH-222)", samsho4_roms, sizeof(samsho4_roms) / sizeof(ROMEntry),
        320, 224, 0
    },
    {
        "samsh5fd", "Samurai Shodown V / Samurai Spirits Zero (Fully Decrypted)", samsh5fd_roms, sizeof(samsh5fd_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "samsho5", "Samurai Shodown V / Samurai Spirits Zero (NGM-2700, set 1)", samsho5_roms, sizeof(samsho5_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_CMC50 | GAME_FLAG_ENCRYPTED_M1
    },
    {
        "ss5spfd", "Samurai Shodown V Special / Samurai Spirits Zero Special (Fully Decrypted)", ss5spfd_roms, sizeof(ss5spfd_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "samsh5sp", "Samurai Shodown V Special / Samurai Spirits Zero Special (NGM-2720)", samsh5sp_roms, sizeof(samsh5sp_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_CMC50 | GAME_FLAG_ENCRYPTED_M1
    },
    {
        "savagere", "Savage Reign / Fu'un Mokushiroku - Kakutou Sousei", savagere_roms, sizeof(savagere_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_SWAPP
    },
    {
        "sengoku", "Sengoku / Sengoku Denshou (NGM-017 ~ NGH-017)", sengoku_roms, sizeof(sengoku_roms) / sizeof(ROMEntry),
        320, 224, 0
    },
    {
        "sengoku2", "Sengoku 2 / Sengoku Denshou 2", sengoku2_roms, sizeof(sengoku2_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_SWAPC
    },
    {
        "sengk3fd", "Sengoku 3 / Sengoku Densho 2001 (Fully Decrypted)", sengk3fd_roms, sizeof(sengk3fd_roms) / sizeof(ROMEntry),
        320, 224, 0
    },
    {
        "sengoku3", "Sengoku 3 / Sengoku Densho 2001 (set 1)", sengoku3_roms, sizeof(sengoku3_roms) / sizeof(ROMEntry),
        320, 224, GAME_FLAG_SWAPP | GAME_FLAG_CMC42
    },
    {
        "shocktro", "Shock Troopers (set 1)", shocktro_roms, sizeof(shocktro_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "shocktr2", "Shock Troopers - 2nd Squad", shocktr2_roms, sizeof(shocktr2_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "moshougi", "Shougi no Tatsujin - Master of Shougi", moshougi_roms, sizeof(moshougi_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "svcfd", "SNK vs. Capcom - SVC Chaos (Fully Decrypted)", svcfd_roms, sizeof(svcfd_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_ALTERNATE_TEXT
    },
    {
        "svc", "SNK vs. Capcom - SVC Chaos (NGM-2690 ~ NGH-2690)", svc_roms, sizeof(svc_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_CMC50 | GAME_FLAG_ALTERNATE_TEXT | GAME_FLAG_ENCRYPTED_M1 | GAME_FLAG_P32
    },
    {
        "socbrawl", "Soccer Brawl (NGM-031)", socbrawl_roms, sizeof(socbrawl_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "spinmast", "Spin Master / Miracle Adventure", spinmast_roms, sizeof(spinmast_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "stakwin", "Stakes Winner / Stakes Winner - GI Kinzen Seiha e no Michi", stakwin_roms, sizeof(stakwin_roms) / sizeof(ROMEntry),
        320, 224, GAME_FLAG_SWAPP
    },
    {
        "stakwin2", "Stakes Winner 2", stakwin2_roms, sizeof(stakwin2_roms) / sizeof(ROMEntry),
        320, 224, GAME_FLAG_SWAPP
    },
    {
        "strhoop", "Street Hoop / Street Slam / Dunk Dream (DEM-004 ~ DEH-004)", strhoop_roms, sizeof(strhoop_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "s1945p", "Strikers 1945 Plus", s1945p_roms, sizeof(s1945p_roms) / sizeof(ROMEntry),
        320, 224, GAME_FLAG_CMC42
    },
    {
        "sdodgeb", "Super Dodge Ball / Kunio no Nekketsu Toukyuu Densetsu", sdodgeb_roms, sizeof(sdodgeb_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_SWAPP
    },
    {
        "ssideki", "Super Sidekicks / Tokuten Ou", ssideki_roms, sizeof(ssideki_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_SWAPC
    },
    {
        "ssideki2", "Super Sidekicks 2 - The World Championship / Tokuten Ou 2 - Real Fight Football (NGM-061 ~ NGH-061)", ssideki2_roms, sizeof(ssideki2_roms) / sizeof(ROMEntry),
        320, 224, 0
    },
    {
        "ssideki3", "Super Sidekicks 3 - The Next Glory / Tokuten Ou 3 - Eikou e no Chousen", ssideki3_roms, sizeof(ssideki3_roms) / sizeof(ROMEntry),
        320, 224, GAME_FLAG_SWAPP
    },
    {
        "twsoc96", "Tecmo World Soccer '96", twsoc96_roms, sizeof(twsoc96_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "irrmaze", "The Irritating Maze / Ultra Denryu Iraira Bou", irrmaze_roms, sizeof(irrmaze_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_SWAPP
    },
    {
        "kof94", "The King of Fighters '94 (NGM-055 ~ NGH-055)", kof94_roms, sizeof(kof94_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_SWAPP
    },
    {
        "kof95", "The King of Fighters '95 (NGM-084)", kof95_roms, sizeof(kof95_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_SWAPP
    },
    {
        "kof96", "The King of Fighters '96 (NGM-214)", kof96_roms, sizeof(kof96_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "kof97", "The King of Fighters '97 (NGM-2320)", kof97_roms, sizeof(kof97_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "kof98", "The King of Fighters '98 - The Slugfest / King of Fighters '98 - Dream Match Never Ends (NGM-2420)", kof98_roms, sizeof(kof98_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "kof98h", "The King of Fighters '98 - The Slugfest / King of Fighters '98 - Dream Match Never Ends (NGH-2420)", kof98h_roms, sizeof(kof98h_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "kof98cb", "The King of Fighters '98 - The Slugfest / King of Fighters '98 - Dream Match Never Ends (Combo, Hack)", kof98cb_roms, sizeof(kof98cb_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "kof99fd", "The King of Fighters '99 - Millennium Battle (Fully Decrypted)", kof99fd_roms, sizeof(kof99fd_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "kof99", "The King of Fighters '99 - Millennium Battle (NGM-2510)", kof99_roms, sizeof(kof99_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_CMC42 | GAME_FLAG_SMA_PROTECTION
    },
    {
        "kof10thd", "The King of Fighters 10th Anniversary (The King of Fighters 2002 bootleg / Fully Decrypted)", kof10thd_roms, sizeof(kof10thd_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "kof2kfd", "The King of Fighters 2000 (Fully Decrypted)", kof2kfd_roms, sizeof(kof2kfd_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_ALTERNATE_TEXT
    },
    {
        "kof2000", "The King of Fighters 2000 (NGM-2570 ~ NGH-2570)", kof2000_roms, sizeof(kof2000_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_CMC50 | GAME_FLAG_ALTERNATE_TEXT | GAME_FLAG_SMA_PROTECTION | GAME_FLAG_ENCRYPTED_M1
    },
    {
        "kof2k1fd", "The King of Fighters 2001 (Fully Decrypted)", kof2k1fd_roms, sizeof(kof2k1fd_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "kof2001", "The King of Fighters 2001 (NGM-262?)", kof2001_roms, sizeof(kof2001_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_CMC50 | GAME_FLAG_ENCRYPTED_M1
    },
    {
        "kof2k2fd", "The King of Fighters 2002 (Fully Decrypted)", kof2k2fd_roms, sizeof(kof2k2fd_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "kof2002", "The King of Fighters 2002 (NGM-2650 ~ NGH-2650)", kof2002_roms, sizeof(kof2002_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_CMC50 | GAME_FLAG_ENCRYPTED_M1
    },
    {
        "kof2k3fd", "The King of Fighters 2003 (Fully Decrypted)", kof2k3fd_roms, sizeof(kof2k3fd_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_ALTERNATE_TEXT
    },
    {
        "kof2003", "The King of Fighters 2003 (NGM-2710, Export)", kof2003_roms, sizeof(kof2003_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_CMC50 | GAME_FLAG_ALTERNATE_TEXT | GAME_FLAG_ENCRYPTED_M1 | GAME_FLAG_P32
    },
    {
        "lastblad", "The Last Blade / Bakumatsu Roman - Gekka no Kenshi (NGM-2340)", lastblad_roms, sizeof(lastblad_roms) / sizeof(ROMEntry),
        320, 224, 0
    },
    {
        "lastbld2", "The Last Blade 2 / Bakumatsu Roman - Dai Ni Maku Gekka no Kenshi (NGM-2430 ~ NGH-2430)", lastbld2_roms, sizeof(lastbld2_roms) / sizeof(ROMEntry),
        320, 224, 0
    },
    {
        "superspy", "The Super Spy (NGM-011 ~ NGH-011)", superspy_roms, sizeof(superspy_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "ssideki4", "The Ultimate 11 - The SNK Football Championship / Tokuten Ou - Honoo no Libero", ssideki4_roms, sizeof(ssideki4_roms) / sizeof(ROMEntry),
        320, 224, GAME_FLAG_SWAPP
    },
    {
        "trally", "Thrash Rally (ALM-003 ~ ALH-003)", trally_roms, sizeof(trally_roms) / sizeof(ROMEntry),
        320, 224, 0
    },
    {
        "tophuntr", "Top Hunter - Roddy & Cathy (NGM-046)", tophuntr_roms, sizeof(tophuntr_roms) / sizeof(ROMEntry),
        320, 224, 0
    },
    {
        "tpgolf", "Top Player's Golf (NGM-003 ~ NGH-003)", tpgolf_roms, sizeof(tpgolf_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "totc", "Treasure of the Caribbean", totc_roms, sizeof(totc_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "twinspri", "Twinkle Star Sprites", twinspri_roms, sizeof(twinspri_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_SWAPP
    },
    {
        "vliner", "V-Liner (v0.7a)", vliner_roms, sizeof(vliner_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "viewpoin", "Viewpoint", viewpoin_roms, sizeof(viewpoin_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_SWAPC
    },
    {
        "gowcaizr", "Voltage Fighter - Gowcaizer / Choujin Gakuen Gowcaizer", gowcaizr_roms, sizeof(gowcaizr_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_SWAPP
    },
    {
        "wakuwak7", "Waku Waku 7", wakuwak7_roms, sizeof(wakuwak7_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "wjammers", "Windjammers / Flying Power Disc", wjammers_roms, sizeof(wjammers_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "wh1", "World Heroes (ALM-005)", wh1_roms, sizeof(wh1_roms) / sizeof(ROMEntry),
        320, 224, GAME_FLAG_SWAPC
    },
    {
        "wh2", "World Heroes 2 (ALM-006 ~ ALH-006)", wh2_roms, sizeof(wh2_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_SWAPP
    },
    {
        "wh2j", "World Heroes 2 Jet (ADM-007 ~ ADH-007)", wh2j_roms, sizeof(wh2j_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_SWAPP
    },
    {
        "whp", "World Heroes Perfect", whp_roms, sizeof(whp_roms) / sizeof(ROMEntry),
        320, 224, GAME_FLAG_SWAPP
    },
    {
        "zedblade", "Zed Blade / Operation Ragnarok", zedblade_roms, sizeof(zedblade_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "zintrckb", "Zintrick / Oshidashi Zentrix (bootleg of CD version)", zintrckb_roms, sizeof(zintrckb_roms) / sizeof(ROMEntry),
        304, 224, 0
    },
    {
        "zupapa", "Zupapa!", zupapa_roms, sizeof(zupapa_roms) / sizeof(ROMEntry),
        304, 224, GAME_FLAG_CMC42
    },
};

const u32 GameDatabase::s_gameCount = static_cast<u32>(sizeof(s_games) / sizeof(GameInfo));

// ============================================================================
// BIOS ROM Definitions
// ============================================================================

// BIOS ROM entries - all 68K BIOS options (indices 0-36) + Z80/Text/Zoom (37-39)
const BIOSROMEntry GameDatabase::s_biosROMs[] = {
    // 68K BIOS options (indices 0-36)
    { "sp-s3.sp1",         0x20000, 0x91b64be3, 0 },  //  0 MVS Asia/Europe ver. 6 (1 slot)
    { "sp-s2.sp1",         0x20000, 0x9036d879, 0 },  //  1 MVS Asia/Europe ver. 5 (1 slot)
    { "sp-s.sp1",          0x20000, 0xc7f2fa45, 0 },  //  2 MVS Asia/Europe ver. 3 (4 slot)
    { "sp-u2.sp1",         0x20000, 0xe72943de, 0 },  //  3 MVS USA ver. 5 (2 slot)
    { "sp1-u2",            0x20000, 0x62f021f4, 0 },  //  4 MVS USA ver. 5 (4 slot)
    { "sp-e.sp1",          0x20000, 0x2723a5b5, 0 },  //  5 MVS USA ver. 5 (6 slot)
    { "sp1-u4.bin",        0x20000, 0x1179a30f, 0 },  //  6 MVS USA (U4)
    { "sp1-u3.bin",        0x20000, 0x2025b7a2, 0 },  //  7 MVS USA (U3)
    { "vs-bios.rom",       0x20000, 0xf0e8f27d, 0 },  //  8 MVS Japan ver. 6 (? slot)
    { "sp-j2.sp1",         0x20000, 0xacede59C, 0 },  //  9 MVS Japan ver. 5 (? slot)
    { "sp1.jipan.1024",    0x20000, 0x9fb0abe4, 0 },  // 10 MVS Japan ver. 3 (4 slot)
    { "sp-45.sp1",         0x80000, 0x03cc9f6a, 0 },  // 11 NEO-MVH MV1C (Asia)
    { "sp-j3.sp1",         0x80000, 0x486cb450, 0 },  // 12 NEO-MVH MV1C (Japan)
    { "japan-j3.bin",      0x20000, 0xdff6d41f, 0 },  // 13 MVS Japan (J3)
    { "sp1-j3.bin",        0x20000, 0xfbc6d469, 0 },  // 14 MVS Japan (J3, alt)
    { "neo-po.bin",        0x20000, 0x16d0c132, 0 },  // 15 AES Japan
    { "neo-epo.bin",       0x20000, 0xd27a71f1, 0 },  // 16 AES Asia
    { "neodebug.bin",      0x20000, 0x698ebb7d, 0 },  // 17 Development Kit
    { "sp-1v1_3db8c.bin",  0x20000, 0x162f0ebe, 0 },  // 18 Deck ver. 6 (Git Ver 1.3)
    { "uni-bios_4_0.rom",  0x20000, 0xa7aab458, 0 },  // 19 Universe BIOS (Hack, Ver. 4.0)
    { "uni-bios_3_3.rom",  0x20000, 0x24858466, 0 },  // 20 Universe BIOS (Hack, Ver. 3.3)
    { "uni-bios_3_2.rom",  0x20000, 0xa4e8b9b3, 0 },  // 21 Universe BIOS (Hack, Ver. 3.2)
    { "uni-bios_3_1.rom",  0x20000, 0x0c58093f, 0 },  // 22 Universe BIOS (Hack, Ver. 3.1)
    { "uni-bios_3_0.rom",  0x20000, 0xa97c89a9, 0 },  // 23 Universe BIOS (Hack, Ver. 3.0)
    { "uni-bios_2_3.rom",  0x20000, 0x27664eb5, 0 },  // 24 Universe BIOS (Hack, Ver. 2.3)
    { "uni-bios_2_3o.rom", 0x20000, 0x601720ae, 0 },  // 25 Universe BIOS (Hack, Ver. 2.3, older?)
    { "uni-bios_2_2.rom",  0x20000, 0x2d50996a, 0 },  // 26 Universe BIOS (Hack, Ver. 2.2)
    { "uni-bios_2_1.rom",  0x20000, 0x8dabf76b, 0 },  // 27 Universe BIOS (Hack, Ver. 2.1)
    { "uni-bios_2_0.rom",  0x20000, 0x0c12c2ad, 0 },  // 28 Universe BIOS (Hack, Ver. 2.0)
    { "uni-bios_1_3.rom",  0x20000, 0xb24b44a0, 0 },  // 29 Universe BIOS (Hack, Ver. 1.3)
    { "uni-bios_1_2.rom",  0x20000, 0x4fa698e9, 0 },  // 30 Universe BIOS (Hack, Ver. 1.2)
    { "uni-bios_1_2o.rom", 0x20000, 0xe19d3ce9, 0 },  // 31 Universe BIOS (Hack, Ver. 1.2, older)
    { "uni-bios_1_1.rom",  0x20000, 0x5dda0d84, 0 },  // 32 Universe BIOS (Hack, Ver. 1.1)
    { "uni-bios_1_0.rom",  0x20000, 0x0ce453a0, 0 },  // 33 Universe BIOS (Hack, Ver. 1.0)
    { "neopen.sp1",        0x20000, 0xcb915e76, 0 },  // 34 NeoOpen BIOS v0.1 beta
    { "",                  0x00000, 0x00000000, 0 },  // 35 Trackball BIOS (placeholder)
    { "",                  0x00000, 0x00000000, 0 },  // 36 PCB BIOS (placeholder)
    
    // Other BIOS ROMs (indices 37-39)
    { "sm1.sm1",           0x20000, 0x94416d67, 0 },  // 37 Z80 BIOS
    { "sfix.sfix",         0x20000, 0xc2ea0cfd, 0 },  // 38 Text layer tiles
    { "000-lo.lo",         0x20000, 0x5a86cff2, 0 },  // 39 Zoom table
};

const u32 GameDatabase::s_biosROMCount = static_cast<u32>(sizeof(s_biosROMs) / sizeof(BIOSROMEntry));

const BIOSROMEntry* GameDatabase::getBIOSROMs() {
    return s_biosROMs;
}

u32 GameDatabase::getBIOSROMCount() {
    return s_biosROMCount;
}

bool GameDatabase::validateBIOSROM(const std::string& filename, const std::vector<u8>& data, const BIOSROMEntry& entry) {
    // Similar to validateROM but for BIOS ROMs
    std::string lowerFilename = filename;
    std::transform(lowerFilename.begin(), lowerFilename.end(), lowerFilename.begin(), ::tolower);
    std::string lowerEntry = entry.filename;
    std::transform(lowerEntry.begin(), lowerEntry.end(), lowerEntry.begin(), ::tolower);
    
    // Try exact match first
    if (lowerFilename == lowerEntry) {
        if (data.size() != entry.size) {
            return false;
        }
        return true;
    }
    
    // Try with .bin extension
    std::string entryBase = lowerEntry;
    size_t dotPos = entryBase.find_last_of('.');
    if (dotPos != std::string::npos) {
        entryBase = entryBase.substr(0, dotPos);
    }
    std::string filenameBase = lowerFilename;
    dotPos = filenameBase.find_last_of('.');
    if (dotPos != std::string::npos) {
        filenameBase = filenameBase.substr(0, dotPos);
    }
    
    if (filenameBase == entryBase) {
        if (data.size() != entry.size) {
            return false;
        }
        return true;
    }
    
    return false;
}

// ============================================================================
// Database Functions
// ============================================================================

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
    
    // No game found
    return nullptr;
}

u32 GameDatabase::calculateCRC32(const std::vector<u8>& data) {
    return static_cast<u32>(mz_crc32(0, data.data(), static_cast<size_t>(data.size())));
}

bool GameDatabase::validateROM(const std::string& filename, const std::vector<u8>& data, const ROMEntry& entry) {
    // Check filename (case-insensitive)
    // Handle both .p1/.sp2/.s1/.c1/.m1/.v1 and .bin extensions
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
    
    // Check CRC32
    u32 calculatedCRC = calculateCRC32(data);
    if (calculatedCRC != entry.crc32) {
        return false;
    }
    
    return true;
}

} // namespace neogeo
