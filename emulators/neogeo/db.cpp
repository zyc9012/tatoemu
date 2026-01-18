#include "db.h"
#include "../../../utilities/miniz/miniz.h"
#include <algorithm>
#include <cstring>

namespace neogeo {

// ============================================================================
// Game Database
// ============================================================================

// KOF94 ROMs
static const ROMEntry kof94_roms[] = {
    { "055-p1.p1",    0x200000, 0xf10a2042, ROMType::PROGRAM, 0 },
    { "055-s1.s1",    0x020000, 0x825976c1, ROMType::TEXT, 0 },
    { "055-c1.c1",    0x200000, 0xb96ef460, ROMType::SPRITE, 0 },
    { "055-c2.c2",    0x200000, 0x15e096a7, ROMType::SPRITE, 0 },
    { "055-c3.c3",    0x200000, 0x54f66254, ROMType::SPRITE, 0 },
    { "055-c4.c4",    0x200000, 0x0b01765f, ROMType::SPRITE, 0 },
    { "055-c5.c5",    0x200000, 0xee759363, ROMType::SPRITE, 0 },
    { "055-c6.c6",    0x200000, 0x498da52c, ROMType::SPRITE, 0 },
    { "055-c7.c7",    0x200000, 0x62f66888, ROMType::SPRITE, 0 },
    { "055-c8.c8",    0x200000, 0xfe0a235d, ROMType::SPRITE, 0 },
    { "055-m1.m1",    0x020000, 0xf6e77cf5, ROMType::SOUND_PROGRAM, 0 },
    { "055-v1.v1",    0x200000, 0x8889596d, ROMType::SOUND_SAMPLE, 0 },
    { "055-v2.v2",    0x200000, 0x25022b27, ROMType::SOUND_SAMPLE, 0 },
    { "055-v3.v3",    0x200000, 0x83cf32c0, ROMType::SOUND_SAMPLE, 0 },
};

// KOF95 ROMs
static const ROMEntry kof95_roms[] = {
    { "084-p1.p1",    0x200000, 0x2cba2716, ROMType::PROGRAM, 0 },
    { "084-s1.s1",    0x020000, 0xde716f8a, ROMType::TEXT, 0 },
    { "084-c1.c1",    0x400000, 0xfe087e32, ROMType::SPRITE, 0 },
    { "084-c2.c2",    0x400000, 0x07864e09, ROMType::SPRITE, 0 },
    { "084-c3.c3",    0x400000, 0xa4e65d1b, ROMType::SPRITE, 0 },
    { "084-c4.c4",    0x400000, 0xc1ace468, ROMType::SPRITE, 0 },
    { "084-c5.c5",    0x200000, 0x8a2c1edc, ROMType::SPRITE, 0 },
    { "084-c6.c6",    0x200000, 0xf593ac35, ROMType::SPRITE, 0 },
    { "084-c7.c7",    0x100000, 0x9904025f, ROMType::SPRITE, 0 },
    { "084-c8.c8",    0x100000, 0x78eb0f9b, ROMType::SPRITE, 0 },
    { "084-m1.m1",    0x020000, 0x6f2d7429, ROMType::SOUND_PROGRAM, 0 },
    { "084-v1.v1",    0x400000, 0x84861b56, ROMType::SOUND_SAMPLE, 0 },
    { "084-v2.v2",    0x200000, 0xb38a2803, ROMType::SOUND_SAMPLE, 0 },
    { "084-v3.v3",    0x100000, 0xd683a338, ROMType::SOUND_SAMPLE, 0 },
};

// KOF96 ROMs
static const ROMEntry kof96_roms[] = {
    { "214-p1.p1",    0x100000, 0x52755d74, ROMType::PROGRAM, 0 },
    { "214-p2.sp2",   0x200000, 0x002ccb73, ROMType::PROGRAM, 0 },
    { "214-s1.s1",    0x020000, 0x1254cbdb, ROMType::TEXT, 0 },
    { "214-c1.c1",    0x400000, 0x7ecf4aa2, ROMType::SPRITE, 0 },
    { "214-c2.c2",    0x400000, 0x05b54f37, ROMType::SPRITE, 0 },
    { "214-c3.c3",    0x400000, 0x64989a65, ROMType::SPRITE, 0 },
    { "214-c4.c4",    0x400000, 0xafbea515, ROMType::SPRITE, 0 },
    { "214-c5.c5",    0x400000, 0x2a3bbd26, ROMType::SPRITE, 0 },
    { "214-c6.c6",    0x400000, 0x44d30dc7, ROMType::SPRITE, 0 },
    { "214-c7.c7",    0x400000, 0x3687331b, ROMType::SPRITE, 0 },
    { "214-c8.c8",    0x400000, 0xfa1461ad, ROMType::SPRITE, 0 },
    { "214-m1.m1",    0x020000, 0xdabc427c, ROMType::SOUND_PROGRAM, 0 },
    { "214-v1.v1",    0x400000, 0x63f7b045, ROMType::SOUND_SAMPLE, 0 },
    { "214-v2.v2",    0x400000, 0x25929059, ROMType::SOUND_SAMPLE, 0 },
    { "214-v3.v3",    0x200000, 0x92a2257d, ROMType::SOUND_SAMPLE, 0 },
};

// KOF97 ROMs
static const ROMEntry kof97_roms[] = {
    { "232-p1.p1",    0x100000, 0x7db81ad9, ROMType::PROGRAM, 0 },
    { "232-p2.sp2",   0x400000, 0x158b23f6, ROMType::PROGRAM, 0 },
    { "232-s1.s1",    0x020000, 0x8514ecf5, ROMType::TEXT, 0 },
    { "232-c1.c1",    0x800000, 0x5f8bf0a1, ROMType::SPRITE, 0 },
    { "232-c2.c2",    0x800000, 0xe4d45c81, ROMType::SPRITE, 0 },
    { "232-c3.c3",    0x800000, 0x581d6618, ROMType::SPRITE, 0 },
    { "232-c4.c4",    0x800000, 0x49bb1e68, ROMType::SPRITE, 0 },
    { "232-c5.c5",    0x400000, 0x34fc4e51, ROMType::SPRITE, 0 },
    { "232-c6.c6",    0x400000, 0x4ff4d47b, ROMType::SPRITE, 0 },
    { "232-m1.m1",    0x020000, 0x45348747, ROMType::SOUND_PROGRAM, 0 },
    { "232-v1.v1",    0x400000, 0x22a2b5b5, ROMType::SOUND_SAMPLE, 0 },
    { "232-v2.v2",    0x400000, 0x2304e744, ROMType::SOUND_SAMPLE, 0 },
    { "232-v3.v3",    0x400000, 0x759eb954, ROMType::SOUND_SAMPLE, 0 },
};

// KOF98 ROMs
static const ROMEntry kof98_roms[] = {
    { "242-p1.p1",    0x200000, 0x8893df89, ROMType::PROGRAM, 0 },
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
    { "242-m1.m1",    0x040000, 0x4ef7016b, ROMType::SOUND_PROGRAM, 0 },
    { "242-v1.v1",    0x400000, 0xb9ea8051, ROMType::SOUND_SAMPLE, 0 },
    { "242-v2.v2",    0x400000, 0xcc11106e, ROMType::SOUND_SAMPLE, 0 },
    { "242-v3.v3",    0x400000, 0x044ea4e1, ROMType::SOUND_SAMPLE, 0 },
    { "242-v4.v4",    0x400000, 0x7985ea30, ROMType::SOUND_SAMPLE, 0 },
};

// KOF99FD ROMs (fully decrypted)
static const ROMEntry kof99fd_roms[] = {
    { "152-p1.p1",    0x100000, 0xf2c7ddfa, ROMType::PROGRAM, 0 },
    { "152-p2.sp2",   0x400000, 0x274ef47a, ROMType::PROGRAM, 0 },
    { "251-s1d.s1",   0x020000, 0x1b0133fe, ROMType::TEXT, 0 },
    { "251-c1d.c1",   0x800000, 0xb3d88546, ROMType::SPRITE, 0 },
    { "251-c2d.c2",   0x800000, 0x915c8634, ROMType::SPRITE, 0 },
    { "251-c3d.c3",   0x800000, 0xb047c9d5, ROMType::SPRITE, 0 },
    { "251-c4d.c4",   0x800000, 0x6bc8e4b1, ROMType::SPRITE, 0 },
    { "251-c5d.c5",   0x800000, 0x9746268c, ROMType::SPRITE, 0 },
    { "251-c6d.c6",   0x800000, 0x238b3e71, ROMType::SPRITE, 0 },
    { "251-c7d.c7",   0x800000, 0x2f68fdeb, ROMType::SPRITE, 0 },
    { "251-c8d.c8",   0x800000, 0x4c2fad1e, ROMType::SPRITE, 0 },
    { "251-m1.m1",    0x020000, 0x5e74539c, ROMType::SOUND_PROGRAM, 0 },
    { "251-v1.v1",    0x400000, 0xef2eecc8, ROMType::SOUND_SAMPLE, 0 },
    { "251-v2.v2",    0x400000, 0x73e211ca, ROMType::SOUND_SAMPLE, 0 },
    { "251-v3.v3",    0x400000, 0x821901da, ROMType::SOUND_SAMPLE, 0 },
    { "251-v4.v4",    0x200000, 0xb49e6178, ROMType::SOUND_SAMPLE, 0 },
};

// KOF2KFD ROMs (fully decrypted)
static const ROMEntry kof2kfd_roms[] = {
    { "257-pg1.p1",   0x100000, 0x5f809dbe, ROMType::PROGRAM, 0 },
    { "257-pg2.sp2",  0x400000, 0x693c2c5e, ROMType::PROGRAM, 0 },
    { "257-c1d.c1",   0x800000, 0xabcdd424, ROMType::SPRITE, 0 },
    { "257-c2d.c2",   0x800000, 0xcda33778, ROMType::SPRITE, 0 },
    { "257-c3d.c3",   0x800000, 0x087fb15b, ROMType::SPRITE, 0 },
    { "257-c4d.c4",   0x800000, 0xfe9dfde4, ROMType::SPRITE, 0 },
    { "257-c5d.c5",   0x800000, 0x03ee4bf4, ROMType::SPRITE, 0 },
    { "257-c6d.c6",   0x800000, 0x8599cc5b, ROMType::SPRITE, 0 },
    { "257-c7d.c7",   0x800000, 0x71dfc3e2, ROMType::SPRITE, 0 },
    { "257-c8d.c8",   0x800000, 0x0fa30e5f, ROMType::SPRITE, 0 },
    { "257-m1d.m1",   0x040000, 0xd404db70, ROMType::SOUND_PROGRAM, 0 },
    { "257-v1.v1",    0x400000, 0x17cde847, ROMType::SOUND_SAMPLE, 0 },
    { "257-v2.v2",    0x400000, 0x1afb20ff, ROMType::SOUND_SAMPLE, 0 },
    { "257-v3.v3",    0x400000, 0x4605036a, ROMType::SOUND_SAMPLE, 0 },
    { "257-v4.v4",    0x400000, 0x764bbd6b, ROMType::SOUND_SAMPLE, 0 },
};

// KOF2K1FD ROMs (fully decrypted)
static const ROMEntry kof2k1fd_roms[] = {
    { "262-pg1.p1",   0x100000, 0x2af7e741, ROMType::PROGRAM, 0 },
    { "262-pg2.sp2",  0x400000, 0x91eea062, ROMType::PROGRAM, 0 },
    { "262-s1d.s1",   0x020000, 0x6d209796, ROMType::TEXT, 0 },
    { "262-c1d.c1",   0x800000, 0x103225b1, ROMType::SPRITE, 0 },
    { "262-c2d.c2",   0x800000, 0xf9d05d99, ROMType::SPRITE, 0 },
    { "262-c3d.c3",   0x800000, 0x4c7ec427, ROMType::SPRITE, 0 },
    { "262-c4d.c4",   0x800000, 0x1d237aa6, ROMType::SPRITE, 0 },
    { "262-c5d.c5",   0x800000, 0xc2256db5, ROMType::SPRITE, 0 },
    { "262-c6d.c6",   0x800000, 0x8d6565a9, ROMType::SPRITE, 0 },
    { "262-c7d.c7",   0x800000, 0xd1408776, ROMType::SPRITE, 0 },
    { "262-c8d.c8",   0x800000, 0x954d0e16, ROMType::SPRITE, 0 },
    { "262-m1d.m1",   0x020000, 0x2fb0a8a5, ROMType::SOUND_PROGRAM, 0 },
    { "262-v1-08-e0.v1", 0x400000, 0x83d49ecf, ROMType::SOUND_SAMPLE, 0 },
    { "262-v2-08-e0.v2", 0x400000, 0x003f1843, ROMType::SOUND_SAMPLE, 0 },
    { "262-v3-08-e0.v3", 0x400000, 0x2ae38dbe, ROMType::SOUND_SAMPLE, 0 },
    { "262-v4-08-e0.v4", 0x400000, 0x26ec4dd9, ROMType::SOUND_SAMPLE, 0 },
};

// KOF2K2FD ROMs (fully decrypted)
static const ROMEntry kof2k2fd_roms[] = {
    { "265-p1.p1",    0x100000, 0x9ede7323, ROMType::PROGRAM, 0 },
    { "265-p2d.sp2",  0x400000, 0x432fdf53, ROMType::PROGRAM, 0 },
    { "265-s1d.s1",   0x020000, 0xe0eaaba3, ROMType::TEXT, 0 },
    { "265-c1d.c1",   0x800000, 0x7efa6ef7, ROMType::SPRITE, 0 },
    { "265-c2d.c2",   0x800000, 0xaa82948b, ROMType::SPRITE, 0 },
    { "265-c3d.c3",   0x800000, 0x959fad0b, ROMType::SPRITE, 0 },
    { "265-c4d.c4",   0x800000, 0xefe6a468, ROMType::SPRITE, 0 },
    { "265-c5d.c5",   0x800000, 0x74bba7c6, ROMType::SPRITE, 0 },
    { "265-c6d.c6",   0x800000, 0xe20d2216, ROMType::SPRITE, 0 },
    { "265-c7d.c7",   0x800000, 0x8a5b561c, ROMType::SPRITE, 0 },
    { "265-c8d.c8",   0x800000, 0xbef667a3, ROMType::SPRITE, 0 },
    { "265-m1d.m1",   0x020000, 0x1c661a4b, ROMType::SOUND_PROGRAM, 0 },
    { "265-v1d.v1",   0x800000, 0x0fc9a58d, ROMType::SOUND_SAMPLE, 0 },
    { "265-v2d.v2",   0x800000, 0xb8c475a4, ROMType::SOUND_SAMPLE, 0 },
};

// KOF2K3FD ROMs (fully decrypted)
static const ROMEntry kof2k3fd_roms[] = {
    { "271-p1d.p1",   0x800000, 0x57a1981d, ROMType::PROGRAM, 0 },
    { "271-s1d.s1",   0x080000, 0x3230e10f, ROMType::TEXT, 0 },
    { "271-c1d.c1",   0x800000, 0xe42fc226, ROMType::SPRITE, 0 },
    { "271-c2d.c2",   0x800000, 0x1b5e3b58, ROMType::SPRITE, 0 },
    { "271-c3d.c3",   0x800000, 0xd334fdd9, ROMType::SPRITE, 0 },
    { "271-c4d.c4",   0x800000, 0x0d457699, ROMType::SPRITE, 0 },
    { "271-c5d.c5",   0x800000, 0x8a91aae4, ROMType::SPRITE, 0 },
    { "271-c6d.c6",   0x800000, 0x9f8674b8, ROMType::SPRITE, 0 },
    { "271-c7d.c7",   0x800000, 0x8ee6b43c, ROMType::SPRITE, 0 },
    { "271-c8d.c8",   0x800000, 0x6d8d2d60, ROMType::SPRITE, 0 },
    { "271-m1d.m1",   0x080000, 0xcc8b54c0, ROMType::SOUND_PROGRAM, 0 },
    { "271-v1d.v1",   0x800000, 0xdd6c6a85, ROMType::SOUND_SAMPLE, 0 },
    { "271-v2d.v2",   0x800000, 0x0e84f8c1, ROMType::SOUND_SAMPLE, 0 },
};

// Fatal Fury 1 ROMs
static const ROMEntry fatfury1_roms[] = {
    { "033-p1.p1",    0x080000, 0x47ebdc2f, ROMType::PROGRAM, 0 },
    { "033-p2.p2",    0x020000, 0xc473af1c, ROMType::PROGRAM, 0 },
    { "033-s1.s1",    0x020000, 0x3c3bdf8c, ROMType::TEXT, 0 },
    { "033-c1.c1",    0x100000, 0x74317e54, ROMType::SPRITE, 0 },
    { "033-c2.c2",    0x100000, 0x5bb952f3, ROMType::SPRITE, 0 },
    { "033-c3.c3",    0x100000, 0x9b714a7c, ROMType::SPRITE, 0 },
    { "033-c4.c4",    0x100000, 0x9397476a, ROMType::SPRITE, 0 },
    { "033-m1.m1",    0x020000, 0x5be10ffd, ROMType::SOUND_PROGRAM, 0 },
    { "033-v1.v1",    0x100000, 0x212fd20d, ROMType::SOUND_SAMPLE, 0 },
    { "033-v2.v2",    0x100000, 0xfa2ae47f, ROMType::SOUND_SAMPLE, 0 },
};

// Fatal Fury 2 ROMs
static const ROMEntry fatfury2_roms[] = {
    { "047-p1.p1",    0x100000, 0xecfdbb69, ROMType::PROGRAM, 0 },
    { "047-s1.s1",    0x020000, 0xd7dbbf39, ROMType::TEXT, 0 },
    { "047-c1.c1",    0x200000, 0xf72a939e, ROMType::SPRITE, 0 },
    { "047-c2.c2",    0x200000, 0x05119a0d, ROMType::SPRITE, 0 },
    { "047-c3.c3",    0x200000, 0x01e00738, ROMType::SPRITE, 0 },
    { "047-c4.c4",    0x200000, 0x9fe27432, ROMType::SPRITE, 0 },
    { "047-m1.m1",    0x020000, 0x820b0ba7, ROMType::SOUND_PROGRAM, 0 },
    { "047-v1.v1",    0x200000, 0xd9d00784, ROMType::SOUND_SAMPLE, 0 },
    { "047-v2.v2",    0x200000, 0x2c9a4b33, ROMType::SOUND_SAMPLE, 0 },
};

// Fatal Fury 3 ROMs
static const ROMEntry fatfury3_roms[] = {
    { "069-p1.p1",    0x100000, 0xa8bcfbbc, ROMType::PROGRAM, 0 },
    { "069-sp2.sp2",  0x200000, 0xdbe963ed, ROMType::PROGRAM, 0 },
    { "069-s1.s1",    0x020000, 0x0b33a800, ROMType::TEXT, 0 },
    { "069-c1.c1",    0x400000, 0xe302f93c, ROMType::SPRITE, 0 },
    { "069-c2.c2",    0x400000, 0x1053a455, ROMType::SPRITE, 0 },
    { "069-c3.c3",    0x400000, 0x1c0fde2f, ROMType::SPRITE, 0 },
    { "069-c4.c4",    0x400000, 0xa25fc3d0, ROMType::SPRITE, 0 },
    { "069-c5.c5",    0x200000, 0xb3ec6fa6, ROMType::SPRITE, 0 },
    { "069-c6.c6",    0x200000, 0x69210441, ROMType::SPRITE, 0 },
    { "069-m1.m1",    0x020000, 0xfce72926, ROMType::SOUND_PROGRAM, 0 },
    { "069-v1.v1",    0x400000, 0x2bdbd4db, ROMType::SOUND_SAMPLE, 0 },
    { "069-v2.v2",    0x400000, 0xa698a487, ROMType::SOUND_SAMPLE, 0 },
    { "069-v3.v3",    0x200000, 0x581c5304, ROMType::SOUND_SAMPLE, 0 },
};

// Fatal Fury Special ROMs
static const ROMEntry fatfursp_roms[] = {
    { "058-p1.p1",    0x100000, 0x2f585ba2, ROMType::PROGRAM, 0 },
    { "058-p2.sp2",   0x080000, 0xd7c71a6b, ROMType::PROGRAM, 0 },
    { "058-s1.s1",    0x020000, 0x2df03197, ROMType::TEXT, 0 },
    { "058-c1.c1",    0x200000, 0x044ab13c, ROMType::SPRITE, 0 },
    { "058-c2.c2",    0x200000, 0x11e6bf96, ROMType::SPRITE, 0 },
    { "058-c3.c3",    0x200000, 0x6f7938d5, ROMType::SPRITE, 0 },
    { "058-c4.c4",    0x200000, 0x4ad066ff, ROMType::SPRITE, 0 },
    { "058-c5.c5",    0x200000, 0x49c5e0bf, ROMType::SPRITE, 0 },
    { "058-c6.c6",    0x200000, 0x8ff1f43d, ROMType::SPRITE, 0 },
    { "058-m1.m1",    0x020000, 0xccc5186e, ROMType::SOUND_PROGRAM, 0 },
    { "058-v1.v1",    0x200000, 0x55d7ce84, ROMType::SOUND_SAMPLE, 0 },
    { "058-v2.v2",    0x200000, 0xee080b10, ROMType::SOUND_SAMPLE, 0 },
    { "058-v3.v3",    0x100000, 0xf9eb3d4a, ROMType::SOUND_SAMPLE, 0 },
};

// Garou: Mark of the Wolves ROMs (fully decrypted)
static const ROMEntry garou_roms[] = {
    { "253-ep1.p1",   0x200000, 0xea3171a4, ROMType::PROGRAM, 0 },
    { "253-ep2.p2",   0x200000, 0x382f704b, ROMType::PROGRAM, 0 },
    { "253-ep3.p3",   0x200000, 0xe395bfdd, ROMType::PROGRAM, 0 },
    { "253-ep4.p4",   0x200000, 0xda92c08e, ROMType::PROGRAM, 0 },
    { "253-c1.c1",    0x800000, 0x0603e046, ROMType::SPRITE, 0 },
    { "253-c2.c2",    0x800000, 0x0917d2a4, ROMType::SPRITE, 0 },
    { "253-c3.c3",    0x800000, 0x6737c92d, ROMType::SPRITE, 0 },
    { "253-c4.c4",    0x800000, 0x5ba92ec6, ROMType::SPRITE, 0 },
    { "253-c5.c5",    0x800000, 0x3eab5557, ROMType::SPRITE, 0 },
    { "253-c6.c6",    0x800000, 0x308d098b, ROMType::SPRITE, 0 },
    { "253-c7.c7",    0x800000, 0xc0e995ae, ROMType::SPRITE, 0 },
    { "253-c8.c8",    0x800000, 0x21a11303, ROMType::SPRITE, 0 },
    { "253-m1.m1",    0x040000, 0x36a806be, ROMType::SOUND_PROGRAM, 0 },
    { "253-v1.v1",    0x400000, 0x263e388c, ROMType::SOUND_SAMPLE, 0 },
    { "253-v2.v2",    0x400000, 0x2c6bc7be, ROMType::SOUND_SAMPLE, 0 },
    { "253-v3.v3",    0x400000, 0x0425b27d, ROMType::SOUND_SAMPLE, 0 },
    { "253-v4.v4",    0x400000, 0xa54be8a9, ROMType::SOUND_SAMPLE, 0 },
};

// Kabuki Klash ROMs
static const ROMEntry kabukikl_roms[] = {
    { "092-p1.p1",    0x200000, 0x28ec9b77, ROMType::PROGRAM, 0 },
    { "092-s1.s1",    0x020000, 0xa3d68ee2, ROMType::TEXT, 0 },
    { "092-c1.c1",    0x400000, 0x2a9fab01, ROMType::SPRITE, 0 },
    { "092-c2.c2",    0x400000, 0x6d2bac02, ROMType::SPRITE, 0 },
    { "092-c3.c3",    0x400000, 0x5da735d6, ROMType::SPRITE, 0 },
    { "092-c4.c4",    0x400000, 0xde07f997, ROMType::SPRITE, 0 },
    { "092-m1.m1",    0x020000, 0x91957ef6, ROMType::SOUND_PROGRAM, 0 },
    { "092-v1.v1",    0x200000, 0x69e90596, ROMType::SOUND_SAMPLE, 0 },
    { "092-v2.v2",    0x200000, 0x7abdb75d, ROMType::SOUND_SAMPLE, 0 },
    { "092-v3.v3",    0x200000, 0xeccc98d3, ROMType::SOUND_SAMPLE, 0 },
    { "092-v4.v4",    0x100000, 0xa7c9c949, ROMType::SOUND_SAMPLE, 0 },
};

// The Last Blade ROMs
static const ROMEntry lastblad_roms[] = {
    { "234-p1.p1",    0x100000, 0xe123a5a3, ROMType::PROGRAM, 0 },
    { "234-p2.sp2",   0x400000, 0x0fdc289e, ROMType::PROGRAM, 0 },
    { "234-s1.s1",    0x020000, 0x95561412, ROMType::TEXT, 0 },
    { "234-c1.c1",    0x800000, 0x9f7e2bd3, ROMType::SPRITE, 0 },
    { "234-c2.c2",    0x800000, 0x80623d3c, ROMType::SPRITE, 0 },
    { "234-c3.c3",    0x800000, 0x91ab1a30, ROMType::SPRITE, 0 },
    { "234-c4.c4",    0x800000, 0x3d60b037, ROMType::SPRITE, 0 },
    { "234-c5.c5",    0x400000, 0x1ba80cee, ROMType::SPRITE, 0 },
    { "234-c6.c6",    0x400000, 0xbeafd091, ROMType::SPRITE, 0 },
    { "234-m1.m1",    0x020000, 0x087628ea, ROMType::SOUND_PROGRAM, 0 },
    { "234-v1.v1",    0x400000, 0xed66b76f, ROMType::SOUND_SAMPLE, 0 },
    { "234-v2.v2",    0x400000, 0xa0e7f6e2, ROMType::SOUND_SAMPLE, 0 },
    { "234-v3.v3",    0x400000, 0xa506e1e2, ROMType::SOUND_SAMPLE, 0 },
    { "234-v4.v4",    0x400000, 0x0e34157f, ROMType::SOUND_SAMPLE, 0 },
};

// The Last Blade 2 ROMs
static const ROMEntry lastbld2_roms[] = {
    { "243-pg1.p1",   0x100000, 0xaf1e6554, ROMType::PROGRAM, 0 },
    { "243-pg2.sp2",  0x400000, 0xadd4a30b, ROMType::PROGRAM, 0 },
    { "243-s1.s1",    0x020000, 0xc9cd2298, ROMType::TEXT, 0 },
    { "243-c1.c1",    0x800000, 0x5839444d, ROMType::SPRITE, 0 },
    { "243-c2.c2",    0x800000, 0xdd087428, ROMType::SPRITE, 0 },
    { "243-c3.c3",    0x800000, 0x6054cbe0, ROMType::SPRITE, 0 },
    { "243-c4.c4",    0x800000, 0x8bd2a9d2, ROMType::SPRITE, 0 },
    { "243-c5.c5",    0x800000, 0x6a503dcf, ROMType::SPRITE, 0 },
    { "243-c6.c6",    0x800000, 0xec9c36d0, ROMType::SPRITE, 0 },
    { "243-m1.m1",    0x020000, 0xacf12d10, ROMType::SOUND_PROGRAM, 0 },
    { "243-v1.v1",    0x400000, 0xf7ee6fbb, ROMType::SOUND_SAMPLE, 0 },
    { "243-v2.v2",    0x400000, 0xaa9e4df6, ROMType::SOUND_SAMPLE, 0 },
    { "243-v3.v3",    0x400000, 0x4ac750b2, ROMType::SOUND_SAMPLE, 0 },
    { "243-v4.v4",    0x400000, 0xf5c64ba6, ROMType::SOUND_SAMPLE, 0 },
};

// Metal Slug ROMs
static const ROMEntry mslug_roms[] = {
    { "201-p1.p1",    0x200000, 0x08d8daa5, ROMType::PROGRAM, 0 },
    { "201-s1.s1",    0x020000, 0x2f55958d, ROMType::TEXT, 0 },
    { "201-c1.c1",    0x400000, 0x72813676, ROMType::SPRITE, 0 },
    { "201-c2.c2",    0x400000, 0x96f62574, ROMType::SPRITE, 0 },
    { "201-c3.c3",    0x400000, 0x5121456a, ROMType::SPRITE, 0 },
    { "201-c4.c4",    0x400000, 0xf4ad59a3, ROMType::SPRITE, 0 },
    { "201-m1.m1",    0x020000, 0xc28b3253, ROMType::SOUND_PROGRAM, 0 },
    { "201-v1.v1",    0x400000, 0x23d22ed1, ROMType::SOUND_SAMPLE, 0 },
    { "201-v2.v2",    0x400000, 0x472cf9db, ROMType::SOUND_SAMPLE, 0 },
};

// Metal Slug 2 ROMs
static const ROMEntry mslug2_roms[] = {
    { "241-p1.p1",    0x100000, 0x2a53c5da, ROMType::PROGRAM, 0 },
    { "241-p2.sp2",   0x200000, 0x38883f44, ROMType::PROGRAM, 0 },
    { "241-s1.s1",    0x020000, 0xf3d32f0f, ROMType::TEXT, 0 },
    { "241-c1.c1",    0x800000, 0x394b5e0d, ROMType::SPRITE, 0 },
    { "241-c2.c2",    0x800000, 0xe5806221, ROMType::SPRITE, 0 },
    { "241-c3.c3",    0x800000, 0x9f6bfa6f, ROMType::SPRITE, 0 },
    { "241-c4.c4",    0x800000, 0x7d3e306f, ROMType::SPRITE, 0 },
    { "241-m1.m1",    0x020000, 0x94520ebd, ROMType::SOUND_PROGRAM, 0 },
    { "241-v1.v1",    0x400000, 0x99ec20e8, ROMType::SOUND_SAMPLE, 0 },
    { "241-v2.v2",    0x400000, 0xecb16799, ROMType::SOUND_SAMPLE, 0 },
};

// Metal Slug X ROMs
static const ROMEntry mslugx_roms[] = {
    { "250-p1.p1",    0x100000, 0x09b8a894, ROMType::PROGRAM, 0 },
    { "250-p2.sp2",   0x400000, 0x1fda2e12, ROMType::PROGRAM, 0 },
    { "250-s1.s1",    0x020000, 0xfb6f441d, ROMType::TEXT, 0 },
    { "250-c1.c1",    0x800000, 0x09a52c6f, ROMType::SPRITE, 0 },
    { "250-c2.c2",    0x800000, 0x31679821, ROMType::SPRITE, 0 },
    { "250-c3.c3",    0x800000, 0x602fdb9a, ROMType::SPRITE, 0 },
    { "250-c4.c4",    0x800000, 0xfd60ff48, ROMType::SPRITE, 0 },
    { "250-c5.c5",    0x800000, 0xfe51cca6, ROMType::SPRITE, 0 },
    { "250-c6.c6",    0x800000, 0x3e9b6aae, ROMType::SPRITE, 0 },
    { "250-m1.m1",    0x020000, 0xfd42a842, ROMType::SOUND_PROGRAM, 0 },
    { "250-v1.v1",    0x400000, 0xc79ede73, ROMType::SOUND_SAMPLE, 0 },
    { "250-v2.v2",    0x400000, 0xea9a4d47, ROMType::SOUND_SAMPLE, 0 },
    { "250-v3.v3",    0x200000, 0x2ca65102, ROMType::SOUND_SAMPLE, 0 },
};

// Metal Slug 3 (Fully Decrypted) ROMs
static const ROMEntry mslug3fd_roms[] = {
    { "256-ph1.p1",   0x100000, 0x9c42ca85, ROMType::PROGRAM, 0 },
    { "256-ph2.sp2",  0x400000, 0x1f3d8ce8, ROMType::PROGRAM, 0 },
    { "256-s1d.s1",   0x020000, 0x8458fff9, ROMType::TEXT, 0 },
    { "256-c1d.c1",   0x800000, 0x3540398c, ROMType::SPRITE, 0 },
    { "256-c2d.c2",   0x800000, 0xbdd220f0, ROMType::SPRITE, 0 },
    { "256-c3d.c3",   0x800000, 0xbfaade82, ROMType::SPRITE, 0 },
    { "256-c4d.c4",   0x800000, 0x1463add6, ROMType::SPRITE, 0 },
    { "256-c5d.c5",   0x800000, 0x48ca7f28, ROMType::SPRITE, 0 },
    { "256-c6d.c6",   0x800000, 0x806eb36f, ROMType::SPRITE, 0 },
    { "256-c7d.c7",   0x800000, 0x9395b809, ROMType::SPRITE, 0 },
    { "256-c8d.c8",   0x800000, 0xa369f9d4, ROMType::SPRITE, 0 },
    { "256-m1.m1",    0x080000, 0xeaeec116, ROMType::SOUND_PROGRAM, 0 },
    { "256-v1.v1",    0x400000, 0xf2690241, ROMType::SOUND_SAMPLE, 0 },
    { "256-v2.v2",    0x400000, 0x7e2a10bd, ROMType::SOUND_SAMPLE, 0 },
    { "256-v3.v3",    0x400000, 0x0eaec17c, ROMType::SOUND_SAMPLE, 0 },
    { "256-v4.v4",    0x400000, 0x9b4b22d4, ROMType::SOUND_SAMPLE, 0 },
};

// Metal Slug 4 (Fully Decrypted) ROMs
static const ROMEntry mslug4fd_roms[] = {
    { "263-p1.p1",    0x100000, 0x27e4def3, ROMType::PROGRAM, 0 },
    { "263-p2.sp2",   0x400000, 0xfdb7aed8, ROMType::PROGRAM, 0 },
    { "263-s1d.s1",   0x020000, 0xa9446774, ROMType::TEXT, 0 },
    { "263-c1d.c1",   0x800000, 0xa75ffcde, ROMType::SPRITE, 0 },
    { "263-c2d.c2",   0x800000, 0x5ab0d12b, ROMType::SPRITE, 0 },
    { "263-c3d.c3",   0x800000, 0x61af560c, ROMType::SPRITE, 0 },
    { "263-c4d.c4",   0x800000, 0xf2c544fd, ROMType::SPRITE, 0 },
    { "263-c5d.c5",   0x800000, 0x84c66c44, ROMType::SPRITE, 0 },
    { "263-c6d.c6",   0x800000, 0x5ed018ab, ROMType::SPRITE, 0 },
    { "263-m1d.m1",   0x020000, 0xef5db532, ROMType::SOUND_PROGRAM, 0 },
    { "263-v1d.v1",   0x400000, 0x8cb5a9ef, ROMType::SOUND_SAMPLE, 0 },
    { "263-v2d.v2",   0x400000, 0x94217b1e, ROMType::SOUND_SAMPLE, 0 },
    { "263-v3d.v3",   0x400000, 0x7616fcec, ROMType::SOUND_SAMPLE, 0 },
    { "263-v4d.v4",   0x400000, 0xc5967f91, ROMType::SOUND_SAMPLE, 0 },
};

// Metal Slug 5 (Fully Decrypted) ROMs
static const ROMEntry mslug5fd_roms[] = {
    { "268-p1d.p1",   0x100000, 0x24ae2e4d, ROMType::PROGRAM, 0 },
    { "268-p2d.sp2",  0x400000, 0x768ee64a, ROMType::PROGRAM, 0 },
    { "268-s1d.s1",   0x020000, 0x64952683, ROMType::TEXT, 0 },
    { "268-c1d.c1",   0x800000, 0xe8239365, ROMType::SPRITE, 0 },
    { "268-c2d.c2",   0x800000, 0x89b21d4c, ROMType::SPRITE, 0 },
    { "268-c3d.c3",   0x800000, 0x3cda13a0, ROMType::SPRITE, 0 },
    { "268-c4d.c4",   0x800000, 0x9c00160d, ROMType::SPRITE, 0 },
    { "268-c5d.c5",   0x800000, 0x38754256, ROMType::SPRITE, 0 },
    { "268-c6d.c6",   0x800000, 0x59d33e9c, ROMType::SPRITE, 0 },
    { "268-c7d.c7",   0x800000, 0xc9f8c357, ROMType::SPRITE, 0 },
    { "268-c8d.c8",   0x800000, 0xfafc3eb9, ROMType::SPRITE, 0 },
    { "268-m1d.m1",   0x020000, 0x346d4a30, ROMType::SOUND_PROGRAM, 0 },
    { "268-v1d.v1",   0x800000, 0x7ff6ca47, ROMType::SOUND_SAMPLE, 0 },
    { "268-v2d.v2",   0x800000, 0x696cce3b, ROMType::SOUND_SAMPLE, 0 },
};

// Real Bout Fatal Fury 1 ROMs
static const ROMEntry rbff1_roms[] = {
    { "095-p1.p1",    0x100000, 0x63b4d8ae, ROMType::PROGRAM, 0 },
    { "095-p2.sp2",   0x200000, 0xcc15826e, ROMType::PROGRAM, 0 },
    { "095-s1.s1",    0x020000, 0xb6bf5e08, ROMType::TEXT, 0 },
    { "069-c1.c1",    0x400000, 0xe302f93c, ROMType::SPRITE, 0 },
    { "069-c2.c2",    0x400000, 0x1053a455, ROMType::SPRITE, 0 },
    { "069-c3.c3",    0x400000, 0x1c0fde2f, ROMType::SPRITE, 0 },
    { "069-c4.c4",    0x400000, 0xa25fc3d0, ROMType::SPRITE, 0 },
    { "095-c5.c5",    0x400000, 0x8b9b65df, ROMType::SPRITE, 0 },
    { "095-c6.c6",    0x400000, 0x3e164718, ROMType::SPRITE, 0 },
    { "095-c7.c7",    0x200000, 0xca605e12, ROMType::SPRITE, 0 },
    { "095-c8.c8",    0x200000, 0x4e6beb6c, ROMType::SPRITE, 0 },
    { "095-m1.m1",    0x020000, 0x653492a7, ROMType::SOUND_PROGRAM, 0 },
    { "069-v1.v1",    0x400000, 0x2bdbd4db, ROMType::SOUND_SAMPLE, 0 },
    { "069-v2.v2",    0x400000, 0xa698a487, ROMType::SOUND_SAMPLE, 0 },
    { "095-v3.v3",    0x400000, 0x189d1c6c, ROMType::SOUND_SAMPLE, 0 },
};

// Real Bout Fatal Fury 2 ROMs
static const ROMEntry rbff2_roms[] = {
    { "240-p1.p1",    0x100000, 0x80e41205, ROMType::PROGRAM, 0 },
    { "240-p2.sp2",   0x400000, 0x960aa88d, ROMType::PROGRAM, 0 },
    { "240-s1.s1",    0x020000, 0xda3b40de, ROMType::TEXT, 0 },
    { "240-c1.c1",    0x800000, 0xeffac504, ROMType::SPRITE, 0 },
    { "240-c2.c2",    0x800000, 0xed182d44, ROMType::SPRITE, 0 },
    { "240-c3.c3",    0x800000, 0x22e0330a, ROMType::SPRITE, 0 },
    { "240-c4.c4",    0x800000, 0xc19a07eb, ROMType::SPRITE, 0 },
    { "240-c5.c5",    0x800000, 0x244dff5a, ROMType::SPRITE, 0 },
    { "240-c6.c6",    0x800000, 0x4609e507, ROMType::SPRITE, 0 },
    { "240-m1.m1",    0x040000, 0xed482791, ROMType::SOUND_PROGRAM, 0 },
    { "240-v1.v1",    0x400000, 0xf796265a, ROMType::SOUND_SAMPLE, 0 },
    { "240-v2.v2",    0x400000, 0x2cb3f3bb, ROMType::SOUND_SAMPLE, 0 },
    { "240-v3.v3",    0x400000, 0x8fe1367a, ROMType::SOUND_SAMPLE, 0 },
    { "240-v4.v4",    0x200000, 0x996704d8, ROMType::SOUND_SAMPLE, 0 },
};

// Real Bout Fatal Fury Special ROMs
static const ROMEntry rbffspec_roms[] = {
    { "223-p1.p1",    0x100000, 0xf84a2d1d, ROMType::PROGRAM, 0 },
    { "223-p2.sp2",   0x400000, 0xaddd8f08, ROMType::PROGRAM, 0 },
    { "223-s1.s1",    0x020000, 0x7ecd6e8c, ROMType::TEXT, 0 },
    { "223-c1.c1",    0x400000, 0xebab05e2, ROMType::SPRITE, 0 },
    { "223-c2.c2",    0x400000, 0x641868c3, ROMType::SPRITE, 0 },
    { "223-c3.c3",    0x400000, 0xca00191f, ROMType::SPRITE, 0 },
    { "223-c4.c4",    0x400000, 0x1f23d860, ROMType::SPRITE, 0 },
    { "223-c5.c5",    0x400000, 0x321e362c, ROMType::SPRITE, 0 },
    { "223-c6.c6",    0x400000, 0xd8fcef90, ROMType::SPRITE, 0 },
    { "223-c7.c7",    0x400000, 0xbc80dd2d, ROMType::SPRITE, 0 },
    { "223-c8.c8",    0x400000, 0x5ad62102, ROMType::SPRITE, 0 },
    { "223-m1.m1",    0x020000, 0x3fee46bf, ROMType::SOUND_PROGRAM, 0 },
    { "223-v1.v1",    0x400000, 0x76673869, ROMType::SOUND_SAMPLE, 0 },
    { "223-v2.v2",    0x400000, 0x7a275acd, ROMType::SOUND_SAMPLE, 0 },
    { "223-v3.v3",    0x400000, 0x5a797fd2, ROMType::SOUND_SAMPLE, 0 },
};

// Samurai Shodown ROMs
static const ROMEntry samsho_roms[] = {
    { "045-p1.p1",    0x100000, 0xdfe51bf0, ROMType::PROGRAM, 0 },
    { "045-pg2.sp2",  0x100000, 0x46745b94, ROMType::PROGRAM, 0 },
    { "045-s1.s1",    0x020000, 0x9142a4d3, ROMType::TEXT, 0 },
    { "045-c1.c1",    0x200000, 0x2e5873a4, ROMType::SPRITE, 0 },
    { "045-c2.c2",    0x200000, 0x04febb10, ROMType::SPRITE, 0 },
    { "045-c3.c3",    0x200000, 0xf3dabd1e, ROMType::SPRITE, 0 },
    { "045-c4.c4",    0x200000, 0x935c62f0, ROMType::SPRITE, 0 },
    { "045-c51.c5",   0x100000, 0x81932894, ROMType::SPRITE, 0 },
    { "045-c61.c6",   0x100000, 0xbe30612e, ROMType::SPRITE, 0 },
    { "045-m1.m1",    0x020000, 0x95170640, ROMType::SOUND_PROGRAM, 0 },
    { "045-v1.v1",    0x200000, 0x37f6d69f, ROMType::SOUND_SAMPLE, 0 },
    { "045-v2.v2",    0x200000, 0x568b20cf, ROMType::SOUND_SAMPLE, 0 },
};

// Samurai Shodown 2 ROMs
static const ROMEntry samsho2_roms[] = {
    { "063-p1.p1",    0x200000, 0x22368892, ROMType::PROGRAM, 0 },
    { "063-s1.s1",    0x020000, 0x64a5cd66, ROMType::TEXT, 0 },
    { "063-c1.c1",    0x200000, 0x86cd307c, ROMType::SPRITE, 0 },
    { "063-c2.c2",    0x200000, 0xcdfcc4ca, ROMType::SPRITE, 0 },
    { "063-c3.c3",    0x200000, 0x7a63ccc7, ROMType::SPRITE, 0 },
    { "063-c4.c4",    0x200000, 0x751025ce, ROMType::SPRITE, 0 },
    { "063-c5.c5",    0x200000, 0x20d3a475, ROMType::SPRITE, 0 },
    { "063-c6.c6",    0x200000, 0xae4c0a88, ROMType::SPRITE, 0 },
    { "063-c7.c7",    0x200000, 0x2df3cbcf, ROMType::SPRITE, 0 },
    { "063-c8.c8",    0x200000, 0x1ffc6dfa, ROMType::SPRITE, 0 },
    { "063-m1.m1",    0x020000, 0x56675098, ROMType::SOUND_PROGRAM, 0 },
    { "063-v1.v1",    0x200000, 0x37703f91, ROMType::SOUND_SAMPLE, 0 },
    { "063-v2.v2",    0x200000, 0x0142bde8, ROMType::SOUND_SAMPLE, 0 },
    { "063-v3.v3",    0x200000, 0xd07fa5ca, ROMType::SOUND_SAMPLE, 0 },
    { "063-v4.v4",    0x100000, 0x24aab4bb, ROMType::SOUND_SAMPLE, 0 },
};

// Samurai Shodown 3 ROMs
static const ROMEntry samsho3_roms[] = {
    { "087-epr.ep1",  0x080000, 0x23e09bb8, ROMType::PROGRAM, 0 },
    { "087-epr.ep2",  0x080000, 0x256f5302, ROMType::PROGRAM, 0 },
    { "087-epr.ep3",  0x080000, 0xbf2db5dd, ROMType::PROGRAM, 0 },
    { "087-epr.ep4",  0x080000, 0x53e60c58, ROMType::PROGRAM, 0 },
    { "087-p5.p5",    0x100000, 0xe86ca4af, ROMType::PROGRAM, 0 },
    { "087-s1.s1",    0x020000, 0x74ec7d9f, ROMType::TEXT, 0 },
    { "087-c1.c1",    0x400000, 0x07a233bc, ROMType::SPRITE, 0 },
    { "087-c2.c2",    0x400000, 0x7a413592, ROMType::SPRITE, 0 },
    { "087-c3.c3",    0x400000, 0x8b793796, ROMType::SPRITE, 0 },
    { "087-c4.c4",    0x400000, 0x728fbf11, ROMType::SPRITE, 0 },
    { "087-c5.c5",    0x400000, 0x172ab180, ROMType::SPRITE, 0 },
    { "087-c6.c6",    0x400000, 0x002ff8f3, ROMType::SPRITE, 0 },
    { "087-c7.c7",    0x100000, 0xae450e3d, ROMType::SPRITE, 0 },
    { "087-c8.c8",    0x100000, 0xa9e82717, ROMType::SPRITE, 0 },
    { "087-m1.m1",    0x020000, 0x8e6440eb, ROMType::SOUND_PROGRAM, 0 },
    { "087-v1.v1",    0x400000, 0x84bdd9a0, ROMType::SOUND_SAMPLE, 0 },
    { "087-v2.v2",    0x200000, 0xac0f261a, ROMType::SOUND_SAMPLE, 0 },
};

// Samurai Shodown 4 ROMs
static const ROMEntry samsho4_roms[] = {
    { "222-p1.p1",    0x100000, 0x1a5cb56d, ROMType::PROGRAM, 0 },
    { "222-p2.sp2",   0x400000, 0xb023cd8b, ROMType::PROGRAM, 0 },
    { "222-s1.s1",    0x020000, 0x8d3d3bf9, ROMType::TEXT, 0 },
    { "222-c1.c1",    0x400000, 0x68f2ed95, ROMType::SPRITE, 0 },
    { "222-c2.c2",    0x400000, 0xa6e9aff0, ROMType::SPRITE, 0 },
    { "222-c3.c3",    0x400000, 0xc91b40f4, ROMType::SPRITE, 0 },
    { "222-c4.c4",    0x400000, 0x359510a4, ROMType::SPRITE, 0 },
    { "222-c5.c5",    0x400000, 0x9cfbb22d, ROMType::SPRITE, 0 },
    { "222-c6.c6",    0x400000, 0x685efc32, ROMType::SPRITE, 0 },
    { "222-c7.c7",    0x400000, 0xd0f86f0d, ROMType::SPRITE, 0 },
    { "222-c8.c8",    0x400000, 0xadfc50e3, ROMType::SPRITE, 0 },
    { "222-m1.m1",    0x020000, 0x7615bc1b, ROMType::SOUND_PROGRAM, 0 },
    { "222-v1.v1",    0x400000, 0x7d6ba95f, ROMType::SOUND_SAMPLE, 0 },
    { "222-v2.v2",    0x400000, 0x6c33bb5d, ROMType::SOUND_SAMPLE, 0 },
    { "222-v3.v3",    0x200000, 0x831ea8c0, ROMType::SOUND_SAMPLE, 0 },
};

// Samurai Shodown 5 ROMs
static const ROMEntry samsho5_roms[] = {
    { "270-p1.p1",    0x400000, 0x4a2a09e6, ROMType::PROGRAM, 0 },
    { "270-p2.sp2",   0x400000, 0xe0c74c85, ROMType::PROGRAM, 0 },
    { "270-c1.c1",    0x800000, 0x14ffffac, ROMType::SPRITE, 0 },
    { "270-c2.c2",    0x800000, 0x401f7299, ROMType::SPRITE, 0 },
    { "270-c3.c3",    0x800000, 0x838f0260, ROMType::SPRITE, 0 },
    { "270-c4.c4",    0x800000, 0x041560a5, ROMType::SPRITE, 0 },
    { "270-c5.c5",    0x800000, 0xbd30b52d, ROMType::SPRITE, 0 },
    { "270-c6.c6",    0x800000, 0x86a69c70, ROMType::SPRITE, 0 },
    { "270-c7.c7",    0x800000, 0xd28fbc3c, ROMType::SPRITE, 0 },
    { "270-c8.c8",    0x800000, 0x02c530a6, ROMType::SPRITE, 0 },
    { "270-m1.m1",    0x080000, 0x49c9901a, ROMType::SOUND_PROGRAM, 0 },
    { "270-v1.v1",    0x800000, 0x62e434eb, ROMType::SOUND_SAMPLE, 0 },
    { "270-v2.v2",    0x800000, 0x180f3c9a, ROMType::SOUND_SAMPLE, 0 },
};

// Samurai Shodown 5 Special ROMs
static const ROMEntry samsh5sph_roms[] = {
    { "272-p1ca.p1",  0x400000, 0xc30a08dd, ROMType::PROGRAM, 0 },
    { "272-p2ca.sp2", 0x400000, 0xbd64a518, ROMType::PROGRAM, 0 },
    { "272-c1.c1",    0x800000, 0x4f97661a, ROMType::SPRITE, 0 },
    { "272-c2.c2",    0x800000, 0xa3afda4f, ROMType::SPRITE, 0 },
    { "272-c3.c3",    0x800000, 0x8c3c7502, ROMType::SPRITE, 0 },
    { "272-c4.c4",    0x800000, 0x32d5e2e2, ROMType::SPRITE, 0 },
    { "272-c5.c5",    0x800000, 0x6ce085bc, ROMType::SPRITE, 0 },
    { "272-c6.c6",    0x800000, 0x05c8dc8e, ROMType::SPRITE, 0 },
    { "272-c7.c7",    0x800000, 0x1417b742, ROMType::SPRITE, 0 },
    { "272-c8.c8",    0x800000, 0xd49773cd, ROMType::SPRITE, 0 },
    { "272-m1.m1",    0x080000, 0xadeebf40, ROMType::SOUND_PROGRAM, 0 },
    { "272-v1.v1",    0x800000, 0x76a94127, ROMType::SOUND_SAMPLE, 0 },
    { "272-v2.v2",    0x800000, 0x4ba507f1, ROMType::SOUND_SAMPLE, 0 },
};

// SVC Chaos (Fully Decrypted) ROMs
static const ROMEntry svcfd_roms[] = {
    { "269-p1d.p1",   0x600000, 0x93855c0b, ROMType::PROGRAM, 0 },
    { "269-s1d.s1",   0x080000, 0xad184232, ROMType::TEXT, 0 },
    { "269-c1d.c1",   0x800000, 0x465d473b, ROMType::SPRITE, 0 },
    { "269-c2d.c2",   0x800000, 0x3eb28f78, ROMType::SPRITE, 0 },
    { "269-c3d.c3",   0x800000, 0xf4d4ab2b, ROMType::SPRITE, 0 },
    { "269-c4d.c4",   0x800000, 0xa69d523a, ROMType::SPRITE, 0 },
    { "269-c5d.c5",   0x800000, 0xba2a7892, ROMType::SPRITE, 0 },
    { "269-c6d.c6",   0x800000, 0x37371ca1, ROMType::SPRITE, 0 },
    { "269-c7d.c7",   0x800000, 0x5595b6cc, ROMType::SPRITE, 0 },
    { "269-c8d.c8",   0x800000, 0xb17dfcf9, ROMType::SPRITE, 0 },
    { "269-m1d.m1",   0x020000, 0x447b3123, ROMType::SOUND_PROGRAM, 0 },
    { "269-v1d.v1",   0x800000, 0xff64cd56, ROMType::SOUND_SAMPLE, 0 },
    { "269-v2d.v2",   0x800000, 0xa8dd6446, ROMType::SOUND_SAMPLE, 0 },
};

// Waku Waku 7 ROMs
static const ROMEntry wakuwak7_roms[] = {
    { "225-p1.p1",    0x100000, 0xb14da766, ROMType::PROGRAM, 0 },
    { "225-p2.sp2",   0x200000, 0xfe190665, ROMType::PROGRAM, 0 },
    { "225-s1.s1",    0x020000, 0x71c4b4b5, ROMType::TEXT, 0 },
    { "225-c1.c1",    0x400000, 0xee4fea54, ROMType::SPRITE, 0 },
    { "225-c2.c2",    0x400000, 0x0c549e2d, ROMType::SPRITE, 0 },
    { "225-c3.c3",    0x400000, 0xaf0897c0, ROMType::SPRITE, 0 },
    { "225-c4.c4",    0x400000, 0x4c66527a, ROMType::SPRITE, 0 },
    { "225-c5.c5",    0x400000, 0x8ecea2b5, ROMType::SPRITE, 0 },
    { "225-c6.c6",    0x400000, 0x0eb11a6d, ROMType::SPRITE, 0 },
    { "225-m1.m1",    0x020000, 0x0634bba6, ROMType::SOUND_PROGRAM, 0 },
    { "225-v1.v1",    0x400000, 0x6195c6b4, ROMType::SOUND_SAMPLE, 0 },
    { "225-v2.v2",    0x400000, 0x6159c5fe, ROMType::SOUND_SAMPLE, 0 },
};

// Game database
const GameInfo GameDatabase::s_games[] = {
    {
        "The King of Fighters '94",
        "kof94",
        kof94_roms,
        sizeof(kof94_roms) / sizeof(ROMEntry),
        GAME_FLAG_SWAPP
    },
    {
        "The King of Fighters '95",
        "kof95",
        kof95_roms,
        sizeof(kof95_roms) / sizeof(ROMEntry),
        GAME_FLAG_SWAPP
    },
    {
        "The King of Fighters '96",
        "kof96",
        kof96_roms,
        sizeof(kof96_roms) / sizeof(ROMEntry),
        0
    },
    {
        "The King of Fighters '97",
        "kof97",
        kof97_roms,
        sizeof(kof97_roms) / sizeof(ROMEntry),
        0
    },
    {
        "The King of Fighters '98",
        "kof98",
        kof98_roms,
        sizeof(kof98_roms) / sizeof(ROMEntry),
        0
    },
    {
        "The King of Fighters '99 (Fully Decrypted)",
        "kof99fd",
        kof99fd_roms,
        sizeof(kof99fd_roms) / sizeof(ROMEntry),
        0
    },
    {
        "The King of Fighters 2000 (Fully Decrypted)",
        "kof2kfd",
        kof2kfd_roms,
        sizeof(kof2kfd_roms) / sizeof(ROMEntry),
        GAME_FLAG_ALTERNATE_TEXT
    },
    {
        "The King of Fighters 2001 (Fully Decrypted)",
        "kof2k1fd",
        kof2k1fd_roms,
        sizeof(kof2k1fd_roms) / sizeof(ROMEntry),
        0
    },
    {
        "The King of Fighters 2002 (Fully Decrypted)",
        "kof2k2fd",
        kof2k2fd_roms,
        sizeof(kof2k2fd_roms) / sizeof(ROMEntry),
        0
    },
    {
        "The King of Fighters 2003 (Fully Decrypted)",
        "kof2k3fd",
        kof2k3fd_roms,
        sizeof(kof2k3fd_roms) / sizeof(ROMEntry),
        GAME_FLAG_ALTERNATE_TEXT
    },
    {
        "Fatal Fury - King of Fighters",
        "fatfury1",
        fatfury1_roms,
        sizeof(fatfury1_roms) / sizeof(ROMEntry),
        0
    },
    {
        "Fatal Fury 2",
        "fatfury2",
        fatfury2_roms,
        sizeof(fatfury2_roms) / sizeof(ROMEntry),
        GAME_FLAG_SWAPC
    },
    {
        "Fatal Fury 3",
        "fatfury3",
        fatfury3_roms,
        sizeof(fatfury3_roms) / sizeof(ROMEntry),
        0
    },
    {
        "Fatal Fury Special",
        "fatfursp",
        fatfursp_roms,
        sizeof(fatfursp_roms) / sizeof(ROMEntry),
        0
    },
    {
        "Garou: Mark of the Wolves",
        "garou",
        garou_roms,
        sizeof(garou_roms) / sizeof(ROMEntry),
        GAME_FLAG_SMA_PROTECTION
    },
    {
        "Kabuki Klash",
        "kabukikl",
        kabukikl_roms,
        sizeof(kabukikl_roms) / sizeof(ROMEntry),
        GAME_FLAG_SWAPP
    },
    {
        "The Last Blade",
        "lastblad",
        lastblad_roms,
        sizeof(lastblad_roms) / sizeof(ROMEntry),
        0
    },
    {
        "The Last Blade 2",
        "lastbld2",
        lastbld2_roms,
        sizeof(lastbld2_roms) / sizeof(ROMEntry),
        0
    },
    {
        "Metal Slug",
        "mslug",
        mslug_roms,
        sizeof(mslug_roms) / sizeof(ROMEntry),
        GAME_FLAG_SWAPP
    },
    {
        "Metal Slug 2",
        "mslug2",
        mslug2_roms,
        sizeof(mslug2_roms) / sizeof(ROMEntry),
        0
    },
    {
        "Metal Slug 3 (Fully Decrypted)",
        "mslug3fd",
        mslug3fd_roms,
        sizeof(mslug3fd_roms) / sizeof(ROMEntry),
        0
    },
    {
        "Metal Slug 4 (Fully Decrypted)",
        "mslug4fd",
        mslug4fd_roms,
        sizeof(mslug4fd_roms) / sizeof(ROMEntry),
        0
    },
    {
        "Metal Slug 5 (Fully Decrypted)",
        "mslug5fd",
        mslug5fd_roms,
        sizeof(mslug5fd_roms) / sizeof(ROMEntry),
        0
    },
    {
        "Metal Slug X",
        "mslugx",
        mslugx_roms,
        sizeof(mslugx_roms) / sizeof(ROMEntry),
        0
    },
    {
        "Real Bout Fatal Fury",
        "rbff1",
        rbff1_roms,
        sizeof(rbff1_roms) / sizeof(ROMEntry),
        0
    },
    {
        "Real Bout Fatal Fury 2",
        "rbff2",
        rbff2_roms,
        sizeof(rbff2_roms) / sizeof(ROMEntry),
        0
    },
    {
        "Real Bout Fatal Fury Special",
        "rbffspec",
        rbffspec_roms,
        sizeof(rbffspec_roms) / sizeof(ROMEntry),
        0
    },
    {
        "Samurai Shodown",
        "samsho",
        samsho_roms,
        sizeof(samsho_roms) / sizeof(ROMEntry),
        0
    },
    {
        "Samurai Shodown II",
        "samsho2",
        samsho2_roms,
        sizeof(samsho2_roms) / sizeof(ROMEntry),
        GAME_FLAG_SWAPP
    },
    {
        "Samurai Shodown III",
        "samsho3",
        samsho3_roms,
        sizeof(samsho3_roms) / sizeof(ROMEntry),
        0
    },
    {
        "Samurai Shodown IV",
        "samsho4",
        samsho4_roms,
        sizeof(samsho4_roms) / sizeof(ROMEntry),
        0
    },
    {
        "Samurai Shodown V",
        "samsho5",
        samsho5_roms,
        sizeof(samsho5_roms) / sizeof(ROMEntry),
        0
    },
    {
        "Samurai Shodown V Special (Fully Decrypted)",
        "samsh5sph",
        samsh5sph_roms,
        sizeof(samsh5sph_roms) / sizeof(ROMEntry),
        0
    },
    {
        "SNK vs. Capcom: SVC Chaos (Fully Decrypted)",
        "svcfd",
        svcfd_roms,
        sizeof(svcfd_roms) / sizeof(ROMEntry),
        0
    },
    {
        "Waku Waku 7",
        "wakuwak7",
        wakuwak7_roms,
        sizeof(wakuwak7_roms) / sizeof(ROMEntry),
        0
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
    
    // Try exact match first
    if (lowerFilename == lowerEntry) {
        // Check size
        if (data.size() != entry.size) {
            return false;
        }
        return true;
    }
    
    // Try with .bin extension (MAME ROM sets often use .bin)
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
        // Check size
        if (data.size() != entry.size) {
            return false;
        }
        return true;
    }
    
    return false;
}

} // namespace neogeo
