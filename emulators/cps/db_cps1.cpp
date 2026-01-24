#include "db.h"
#include "../../../utilities/miniz/miniz.h"
#include <algorithm>
#include <cstring>

namespace cps {

// Street Fighter II: The World Warrior (sf2)
static const ROMEntry sf2_roms[] = {
    { "sf2e_30g.11e",  0x020000, 0xfe39ee33, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "sf2e_37g.11f",  0x020000, 0xfb92cd74, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "sf2e_31g.12e",  0x020000, 0x69a0a301, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "sf2e_38g.12f",  0x020000, 0x5e22db70, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "sf2e_28g.9e",   0x020000, 0x8bf9f1e5, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "sf2e_35g.9f",   0x020000, 0x626ef934, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "sf2_29b.10e",   0x020000, 0xbb4af315, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "sf2_36b.10f",   0x020000, 0xc02a13eb, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
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
    { "sf2_9.12a",     0x010000, 0xa4823a1b, ROMType::SOUND_PROGRAM, 0 },
    { "sf2_18.11c",    0x020000, 0x7f162009, ROMType::SOUND_SAMPLE, 0 },
    { "sf2_19.12c",    0x020000, 0xbeade53f, ROMType::SOUND_SAMPLE, 0 },
};

// Street Fighter II: Champion Edition (sf2ce)
static const ROMEntry sf2ce_roms[] = {
    { "s92e_23b.8f",   0x080000, 0x0aaa1a3a, ROMType::PROGRAM, 0 },
    { "s92_22b.7f",    0x080000, 0x2bbe15ed, ROMType::PROGRAM, 0 },
    { "s92_21a.6f",    0x080000, 0x925a7877, ROMType::PROGRAM, 0 },
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
    { "s92_09.11a",    0x010000, 0x08f6b60e, ROMType::SOUND_PROGRAM, 0 },
    { "s92_18.11c",    0x020000, 0x7f162009, ROMType::SOUND_SAMPLE, 0 },
    { "s92_19.12c",    0x020000, 0xbeade53f, ROMType::SOUND_SAMPLE, 0 },
};

// Street Fighter II: Hyper Fighting (sf2hf)
static const ROMEntry sf2hf_roms[] = {
    { "s2te_23.8f",    0x080000, 0x2dd72514, ROMType::PROGRAM, 0 },
    { "s2te_22.7f",    0x080000, 0xaea6e035, ROMType::PROGRAM, 0 },
    { "s2te_21.6f",    0x080000, 0xfd200288, ROMType::PROGRAM, 0 },
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
    { "s92_09.11a",    0x010000, 0x08f6b60e, ROMType::SOUND_PROGRAM, 0 },
    { "s92_18.11c",    0x020000, 0x7f162009, ROMType::SOUND_SAMPLE, 0 },
    { "s92_19.12c",    0x020000, 0xbeade53f, ROMType::SOUND_SAMPLE, 0 },
};

// Three Wonders (3wonders)
static const ROMEntry threewonders_roms[] = {
    { "rte_30a.11f",   0x020000, 0xef5b8b33, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "rte_35a.11h",   0x020000, 0x7d705529, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "rte_31a.12f",   0x020000, 0x32835e5e, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "rte_36a.12h",   0x020000, 0x7637975f, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "rt_28a.9f",     0x020000, 0x054137c8, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "rt_33a.9h",     0x020000, 0x7264cb1b, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "rte_29a.10f",   0x020000, 0xcddaa919, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "rte_34a.10h",   0x020000, 0xed52e7e5, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "rt-5m.7a",      0x080000, 0x86aef804, ROMType::GRAPHICS, 0 },
    { "rt-7m.9a",      0x080000, 0x4f057110, ROMType::GRAPHICS, 0 },
    { "rt-1m.3a",      0x080000, 0x902489d0, ROMType::GRAPHICS, 0 },
    { "rt-3m.5a",      0x080000, 0xe35ce720, ROMType::GRAPHICS, 0 },
    { "rt-6m.8a",      0x080000, 0x13cb0e7c, ROMType::GRAPHICS, 0 },
    { "rt-8m.10a",     0x080000, 0x1f055014, ROMType::GRAPHICS, 0 },
    { "rt-2m.4a",      0x080000, 0xe9a034f4, ROMType::GRAPHICS, 0 },
    { "rt-4m.6a",      0x080000, 0xdf0eea8b, ROMType::GRAPHICS, 0 },
    { "rt_9.12b",      0x010000, 0xabfca165, ROMType::SOUND_PROGRAM, 0 },
    { "rt_18.11c",     0x020000, 0x26b211ab, ROMType::SOUND_SAMPLE, 0 },
    { "rt_19.12c",     0x020000, 0xdbe64ad0, ROMType::SOUND_SAMPLE, 0 },
};

// Captain Commando (captcomm)
static const ROMEntry captcomm_roms[] = {
    { "cce_23f.8f",    0x080000, 0x42c814c5, ROMType::PROGRAM, 0 },
    { "cc_22f.7f",     0x080000, 0x0fd34195, ROMType::PROGRAM, 0 },
    { "cc_24f.9e",     0x020000, 0x3a794f25, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "cc_28f.9f",     0x020000, 0xfc3c2906, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "cc-5m.3a",      0x080000, 0x7261d8ba, ROMType::GRAPHICS, 0 },
    { "cc-7m.5a",      0x080000, 0x6a60f949, ROMType::GRAPHICS, 0 },
    { "cc-1m.4a",      0x080000, 0x00637302, ROMType::GRAPHICS, 0 },
    { "cc-3m.6a",      0x080000, 0xcc87cf61, ROMType::GRAPHICS, 0 },
    { "cc-6m.7a",      0x080000, 0x28718bed, ROMType::GRAPHICS, 0 },
    { "cc-8m.9a",      0x080000, 0xd4acc53a, ROMType::GRAPHICS, 0 },
    { "cc-2m.8a",      0x080000, 0x0c69f151, ROMType::GRAPHICS, 0 },
    { "cc-4m.10a",     0x080000, 0x1f9ebb97, ROMType::GRAPHICS, 0 },
    { "cc_09.11a",     0x010000, 0x698e8b58, ROMType::SOUND_PROGRAM, 0 },
    { "cc_18.11c",     0x020000, 0x6de2c2db, ROMType::SOUND_SAMPLE, 0 },
    { "cc_19.12c",     0x020000, 0xb99091ae, ROMType::SOUND_SAMPLE, 0 },
};

// Knights of the Round (knights)
static const ROMEntry knights_roms[] = {
    { "kr_23e.8f",     0x080000, 0x1b3997eb, ROMType::PROGRAM, 0 },
    { "kr_22.7f",      0x080000, 0xd0b671a9, ROMType::PROGRAM, 0 },
    { "kr-5m.3a",      0x080000, 0x9e36c1a4, ROMType::GRAPHICS, 0 },
    { "kr-7m.5a",      0x080000, 0xc5832cae, ROMType::GRAPHICS, 0 },
    { "kr-1m.4a",      0x080000, 0xf095be2d, ROMType::GRAPHICS, 0 },
    { "kr-3m.6a",      0x080000, 0x179dfd96, ROMType::GRAPHICS, 0 },
    { "kr-6m.7a",      0x080000, 0x1f4298d2, ROMType::GRAPHICS, 0 },
    { "kr-8m.9a",      0x080000, 0x37fa8751, ROMType::GRAPHICS, 0 },
    { "kr-2m.8a",      0x080000, 0x0200bc3d, ROMType::GRAPHICS, 0 },
    { "kr-4m.10a",     0x080000, 0x0bb2b4e7, ROMType::GRAPHICS, 0 },
    { "kr_09.11a",     0x010000, 0x5e44d9ee, ROMType::SOUND_PROGRAM, 0 },
    { "kr_18.11c",     0x020000, 0xda69d15f, ROMType::SOUND_SAMPLE, 0 },
    { "kr_19.12c",     0x020000, 0xbfc654e9, ROMType::SOUND_SAMPLE, 0 },
};

// The King of Dragons (kod)
static const ROMEntry kod_roms[] = {
    { "kde_30a.11e",   0x020000, 0xfcb5efe2, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "kde_37a.11f",   0x020000, 0xf22e5266, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "kde_31a.12e",   0x020000, 0xc710d722, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "kde_38a.12f",   0x020000, 0x57d6ed3a, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "kd_28.9e",      0x020000, 0x9367bcd9, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "kd_35.9f",      0x020000, 0x4ca6a48a, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "kd_29.10e",     0x020000, 0x0360fa72, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "kd_36a.10f",    0x020000, 0x95a3cef8, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "kd-5m.4a",      0x080000, 0xe45b8701, ROMType::GRAPHICS, 0 },
    { "kd-7m.6a",      0x080000, 0xa7750322, ROMType::GRAPHICS, 0 },
    { "kd-1m.3a",      0x080000, 0x5f74bf78, ROMType::GRAPHICS, 0 },
    { "kd-3m.5a",      0x080000, 0x5e5303bf, ROMType::GRAPHICS, 0 },
    { "kd-6m.4c",      0x080000, 0x113358f3, ROMType::GRAPHICS, 0 },
    { "kd-8m.6c",      0x080000, 0x38853c44, ROMType::GRAPHICS, 0 },
    { "kd-2m.3c",      0x080000, 0x9ef36604, ROMType::GRAPHICS, 0 },
    { "kd-4m.5c",      0x080000, 0x402b9b4f, ROMType::GRAPHICS, 0 },
    { "kd_9.12a",      0x010000, 0xbac6ec26, ROMType::SOUND_PROGRAM, 0 },
    { "kd_18.11c",     0x020000, 0x4c63181d, ROMType::SOUND_SAMPLE, 0 },
    { "kd_19.12c",     0x020000, 0x92941b80, ROMType::SOUND_SAMPLE, 0 },
};

// Nemo (nemo)
static const ROMEntry nemo_roms[] = {
    { "nme_30a.11f",   0x020000, 0xd2c03e56, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "nme_35a.11h",   0x020000, 0x5fd31661, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "nme_31a.12f",   0x020000, 0xb2bd4f6f, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "nme_36a.12h",   0x020000, 0xee9450e3, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "nm-32m.8h",     0x080000, 0xd6d1add3, ROMType::PROGRAM, 0 },
    { "nm-5m.7a",      0x080000, 0x487b8747, ROMType::GRAPHICS, 0 },
    { "nm-7m.9a",      0x080000, 0x203dc8c6, ROMType::GRAPHICS, 0 },
    { "nm-1m.3a",      0x080000, 0x9e878024, ROMType::GRAPHICS, 0 },
    { "nm-3m.5a",      0x080000, 0xbb01e6b6, ROMType::GRAPHICS, 0 },
    { "nme_09.12b",    0x010000, 0x0f4b0581, ROMType::SOUND_PROGRAM, 0 },
    { "nme_18.11c",    0x020000, 0xbab333d4, ROMType::SOUND_SAMPLE, 0 },
    { "nme_19.12c",    0x020000, 0x2650a0a8, ROMType::SOUND_SAMPLE, 0 },
};

// Final Fight (ffight)
static const ROMEntry ffight_roms[] = {
    { "ff_36.11f",     0x020000, 0xf9a5ce83, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "ff_42.11h",     0x020000, 0x65f11215, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "ff_37.12f",     0x020000, 0xe1033784, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "ffe_43.12h",    0x020000, 0x995e968a, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "ff-32m.8h",     0x080000, 0xc747696e, ROMType::PROGRAM, 0 },
    { "ff-5m.7a",      0x080000, 0x9c284108, ROMType::GRAPHICS, 0 },
    { "ff-7m.9a",      0x080000, 0xa7584dfb, ROMType::GRAPHICS, 0 },
    { "ff-1m.3a",      0x080000, 0x0b605e44, ROMType::GRAPHICS, 0 },
    { "ff-3m.5a",      0x080000, 0x52291cd2, ROMType::GRAPHICS, 0 },
    { "ff_09.12b",     0x010000, 0xb8367eb5, ROMType::SOUND_PROGRAM, 0 },
    { "ff_18.11c",     0x020000, 0x375c66e7, ROMType::SOUND_SAMPLE, 0 },
    { "ff_19.12c",     0x020000, 0x1ef137f9, ROMType::SOUND_SAMPLE, 0 },
};

// Warriors of Fate / Tenchi wo Kurau II (wof)
static const ROMEntry wof_roms[] = {
    { "tk2e_23c.8f",   0x080000, 0x0d708505, ROMType::PROGRAM, 0 },
    { "tk2e_22c.7f",   0x080000, 0x608c17e3, ROMType::PROGRAM, 0 },
    { "tk2-1m.3a",     0x080000, 0x0d9cb9bf, ROMType::GRAPHICS, 0 },
    { "tk2-3m.5a",     0x080000, 0x45227027, ROMType::GRAPHICS, 0 },
    { "tk2-2m.4a",     0x080000, 0xc5ca2460, ROMType::GRAPHICS, 0 },
    { "tk2-4m.6a",     0x080000, 0xe349551c, ROMType::GRAPHICS, 0 },
    { "tk2-5m.7a",     0x080000, 0x291f0f0b, ROMType::GRAPHICS, 0 },
    { "tk2-7m.9a",     0x080000, 0x3edeb949, ROMType::GRAPHICS, 0 },
    { "tk2-6m.8a",     0x080000, 0x1abd14d6, ROMType::GRAPHICS, 0 },
    { "tk2-8m.10a",    0x080000, 0xb27948e3, ROMType::GRAPHICS, 0 },
    { "tk2_qa.5k",     0x020000, 0xc9183a0d, ROMType::SOUND_PROGRAM, 0 },
    { "tk2-q1.1k",     0x080000, 0x611268cf, ROMType::SOUND_SAMPLE, 0 },
    { "tk2-q2.2k",     0x080000, 0x20f55ca9, ROMType::SOUND_SAMPLE, 0 },
    { "tk2-q3.3k",     0x080000, 0xbfcf6f52, ROMType::SOUND_SAMPLE, 0 },
    { "tk2-q4.4k",     0x080000, 0x36642e88, ROMType::SOUND_SAMPLE, 0 },
};

// The Punisher (punisher)
static const ROMEntry punisher_roms[] = {
    { "pse_26.11e",    0x020000, 0x389a99d2, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "pse_30.11f",    0x020000, 0x68fb06ac, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "pse_27.12e",    0x020000, 0x3eb181c3, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "pse_31.12f",    0x020000, 0x37108e7b, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "pse_24.9e",     0x020000, 0x0f434414, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "pse_28.9f",     0x020000, 0xb732345d, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "pse_25.10e",    0x020000, 0xb77102e2, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "pse_29.10f",    0x020000, 0xec037bce, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "ps_21.6f",      0x080000, 0x8affa5a9, ROMType::PROGRAM, 0 },
    { "ps-1m.3a",      0x080000, 0x77b7ccab, ROMType::GRAPHICS, 0 },
    { "ps-3m.5a",      0x080000, 0x0122720b, ROMType::GRAPHICS, 0 },
    { "ps-2m.4a",      0x080000, 0x64fa58d4, ROMType::GRAPHICS, 0 },
    { "ps-4m.6a",      0x080000, 0x60da42c8, ROMType::GRAPHICS, 0 },
    { "ps-5m.7a",      0x080000, 0xc54ea839, ROMType::GRAPHICS, 0 },
    { "ps-7m.9a",      0x080000, 0x04c5acbd, ROMType::GRAPHICS, 0 },
    { "ps-6m.8a",      0x080000, 0xa544f4cc, ROMType::GRAPHICS, 0 },
    { "ps-8m.10a",     0x080000, 0x8f02f436, ROMType::GRAPHICS, 0 },
    { "ps_q.5k",       0x020000, 0x49ff4446, ROMType::SOUND_PROGRAM, 0 },
    { "ps-q1.1k",      0x080000, 0x31fd8726, ROMType::SOUND_SAMPLE, 0 },
    { "ps-q2.2k",      0x080000, 0x980a9eef, ROMType::SOUND_SAMPLE, 0 },
    { "ps-q3.3k",      0x080000, 0x0dd44491, ROMType::SOUND_SAMPLE, 0 },
    { "ps-q4.4k",      0x080000, 0xbed42f03, ROMType::SOUND_SAMPLE, 0 },
};

// Mega Man: The Power Battle (megaman)
static const ROMEntry megaman_roms[] = {
    { "rcmu_23b.8f",   0x080000, 0x1cd33c7a, ROMType::PROGRAM, 0 },
    { "rcmu_22b.7f",   0x080000, 0x708268c4, ROMType::PROGRAM, 0 },
    { "rcmu_21a.6f",   0x080000, 0x4376ea95, ROMType::PROGRAM, 0 },
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
    { "rcm_09.11a",    0x010000, 0x22ac8f5f, ROMType::SOUND_PROGRAM, 0 },
    { "rcm_18.11c",    0x020000, 0x80f1f8aa, ROMType::SOUND_SAMPLE, 0 },
    { "rcm_19.12c",    0x020000, 0xf257dbe1, ROMType::SOUND_SAMPLE, 0 },
};

// Willow (willow)
static const ROMEntry willow_roms[] = {
    { "wle_30.11f",    0x020000, 0x15372aa2, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "wle_35.11h",    0x020000, 0x2e64623b, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "wlu_31.12f",    0x020000, 0x0eb48a83, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "wlu_36.12h",    0x020000, 0x36100209, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "wlm-32.8h",     0x080000, 0xdfd9f643, ROMType::PROGRAM, 0 },
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
    { "wl_09.12b",     0x010000, 0xf6b3d060, ROMType::SOUND_PROGRAM, 0 },
    { "wl_18.11c",     0x020000, 0xbde23d4d, ROMType::SOUND_SAMPLE, 0 },
    { "wl_19.12c",     0x020000, 0x683898f5, ROMType::SOUND_SAMPLE, 0 },
};

// Mercs (mercs)
static const ROMEntry mercs_roms[] = {
    { "so2_30e.11f",   0x020000, 0xe17f9bf7, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "so2_35e.11h",   0x020000, 0x78e63575, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "so2_31e.12f",   0x020000, 0x51204d36, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "so2_36e.12h",   0x020000, 0x9cfba8b4, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "so2-32m.8h",    0x080000, 0x2eb5cf0c, ROMType::PROGRAM, 0 },
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
    { "so2_09.12b",    0x010000, 0xd09d7c7a, ROMType::SOUND_PROGRAM, 0 },
    { "so2_18.11c",    0x020000, 0xbbea1643, ROMType::SOUND_SAMPLE, 0 },
    { "so2_19.12c",    0x020000, 0xac58aa71, ROMType::SOUND_SAMPLE, 0 },
};

// Varth: Operation Thunderstorm (varth)
static const ROMEntry varth_roms[] = {
    { "vae_30b.11f",   0x020000, 0xadb8d391, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "vae_35b.11h",   0x020000, 0x44e5548f, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "vae_31b.12f",   0x020000, 0x1749a71c, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "vae_36b.12h",   0x020000, 0x5f2e2450, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "vae_28b.9f",    0x020000, 0xe524ca50, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "vae_33b.9h",    0x020000, 0xc0bbf8c9, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "vae_29b.10f",   0x020000, 0x6640996a, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "vae_34b.10h",   0x020000, 0xfa59be8a, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "va-5m.7a",      0x080000, 0xb1fb726e, ROMType::GRAPHICS, 0 },
    { "va-7m.9a",      0x080000, 0x4c6588cd, ROMType::GRAPHICS, 0 },
    { "va-1m.3a",      0x080000, 0x0b1ace37, ROMType::GRAPHICS, 0 },
    { "va-3m.5a",      0x080000, 0x44dfe706, ROMType::GRAPHICS, 0 },
    { "va_09.12b",     0x010000, 0x7a99446e, ROMType::SOUND_PROGRAM, 0 },
    { "va_18.11c",     0x020000, 0xde30510e, ROMType::SOUND_SAMPLE, 0 },
    { "va_19.12c",     0x020000, 0x0610a4ac, ROMType::SOUND_SAMPLE, 0 },
};

// Carrier Air Wing (cawing)
static const ROMEntry cawing_roms[] = {
    { "cae_30a.11f",   0x020000, 0x91fceacd, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "cae_35a.11h",   0x020000, 0x3ef03083, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "cae_31a.12f",   0x020000, 0xe5b75caf, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "cae_36a.12h",   0x020000, 0xc73fd713, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "ca-32m.8h",     0x080000, 0x0c4837d4, ROMType::PROGRAM, 0 },
    { "ca-5m.7a",      0x080000, 0x66d4cc37, ROMType::GRAPHICS, 0 },
    { "ca-7m.9a",      0x080000, 0xb6f896f2, ROMType::GRAPHICS, 0 },
    { "ca-1m.3a",      0x080000, 0x4d0620fd, ROMType::GRAPHICS, 0 },
    { "ca-3m.5a",      0x080000, 0x0b0341c3, ROMType::GRAPHICS, 0 },
    { "ca_9.12b",      0x010000, 0x96fe7485, ROMType::SOUND_PROGRAM, 0 },
    { "ca_18.11c",     0x020000, 0x4a613a2c, ROMType::SOUND_SAMPLE, 0 },
    { "ca_19.12c",     0x020000, 0x74584493, ROMType::SOUND_SAMPLE, 0 },
};

// 1941: Counter Attack (1941)
static const ROMEntry game1941_roms[] = {
    { "41em_30.11f",   0x020000, 0x4249ec61, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "41em_35.11h",   0x020000, 0xddbee5eb, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "41em_31.12f",   0x020000, 0x584e88e5, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "41em_36.12h",   0x020000, 0x3cfc31d0, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "41-32m.8h",     0x080000, 0x4e9648ca, ROMType::PROGRAM, 0 },
    { "41-5m.7a",      0x080000, 0x01d1cb11, ROMType::GRAPHICS, 0 },
    { "41-7m.9a",      0x080000, 0xaeaa3509, ROMType::GRAPHICS, 0 },
    { "41-1m.3a",      0x080000, 0xff77985a, ROMType::GRAPHICS, 0 },
    { "41-3m.5a",      0x080000, 0x983be58f, ROMType::GRAPHICS, 0 },
    { "41_9.12b",      0x010000, 0x0f9d8527, ROMType::SOUND_PROGRAM, 0 },
    { "41_18.11c",     0x020000, 0xd1f15aeb, ROMType::SOUND_SAMPLE, 0 },
    { "41_19.12c",     0x020000, 0x15aec3a6, ROMType::SOUND_SAMPLE, 0 },
};

// Magic Sword (msword)
static const ROMEntry msword_roms[] = {
    { "mse_30.11f",    0x020000, 0x03fc8dbc, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "mse_35.11h",    0x020000, 0xd5bf66cd, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "mse_31.12f",    0x020000, 0x30332bcf, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "mse_36.12h",    0x020000, 0x8f7d6ce9, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "ms-32m.8h",     0x080000, 0x2475ddfc, ROMType::PROGRAM, 0 },
    { "ms-5m.7a",      0x080000, 0xc00fe7e2, ROMType::GRAPHICS, 0 },
    { "ms-7m.9a",      0x080000, 0x4ccacac5, ROMType::GRAPHICS, 0 },
    { "ms-1m.3a",      0x080000, 0x0d2bbe00, ROMType::GRAPHICS, 0 },
    { "ms-3m.5a",      0x080000, 0x3a1a5bf4, ROMType::GRAPHICS, 0 },
    { "ms_09.12b",     0x010000, 0x57b29519, ROMType::SOUND_PROGRAM, 0 },
    { "ms_18.11c",     0x020000, 0xfb64e90d, ROMType::SOUND_SAMPLE, 0 },
    { "ms_19.12c",     0x020000, 0x74f892b9, ROMType::SOUND_SAMPLE, 0 },
};

// Area 88
static const ROMEntry area88_roms[] = {
    { "ar_36.12f",     0x020000, 0x65030392, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "ar_42.12h",     0x020000, 0xc48170de, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "ar_37.13f",     0x020000, 0x33e9694b, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "ar_43.13h",     0x020000, 0x7cc8fb9e, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "ar_34.10f",     0x020000, 0xf6e80386, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "ar_40.10h",     0x020000, 0xbe36c145, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "ar_35.11f",     0x020000, 0x86d98ff3, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "ar_41.11h",     0x020000, 0x758893d3, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "ar_09.4b",      0x020000, 0xdb9376f8, ROMType::GRAPHICS, 0 },
    { "ar_01.4a",      0x020000, 0x392151b4, ROMType::GRAPHICS, 0 },
    { "ar_13.9b",      0x020000, 0x81436481, ROMType::GRAPHICS, 0 },
    { "ar_05.9a",      0x020000, 0xe246ed9f, ROMType::GRAPHICS, 0 },
    { "ar_24.5e",      0x020000, 0x9cd6e2a3, ROMType::GRAPHICS, 0 },
    { "ar_17.5c",      0x020000, 0x0b8e0df4, ROMType::GRAPHICS, 0 },
    { "ar_38.8h",      0x020000, 0x8b9e75b9, ROMType::GRAPHICS, 0 },
    { "ar_32.8f",      0x020000, 0xdb6acdcf, ROMType::GRAPHICS, 0 },
    { "ar_10.5b",      0x020000, 0x4219b622, ROMType::GRAPHICS, 0 },
    { "ar_02.5a",      0x020000, 0xbac5dec5, ROMType::GRAPHICS, 0 },
    { "ar_14.10b",     0x020000, 0xe6bae179, ROMType::GRAPHICS, 0 },
    { "ar_06.10a",     0x020000, 0xc8f04223, ROMType::GRAPHICS, 0 },
    { "ar_25.7e",      0x020000, 0x15ccf981, ROMType::GRAPHICS, 0 },
    { "ar_18.7c",      0x020000, 0x9336db6a, ROMType::GRAPHICS, 0 },
    { "ar_39.9h",      0x020000, 0x9b8e1363, ROMType::GRAPHICS, 0 },
    { "ar_33.9f",      0x020000, 0x3968f4b5, ROMType::GRAPHICS, 0 },
    { "ar_23.13c",     0x010000, 0xf3dd1367, ROMType::SOUND_PROGRAM, 0 },
    { "ar_30.12e",     0x020000, 0x584b43a9, ROMType::SOUND_SAMPLE, 0 },
};

// Adventure Quiz Capcom World 2
static const ROMEntry cworld2j_roms[] = {
    { "q5_36.12f",     0x020000, 0x38a08099, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "q5_42.12h",     0x020000, 0x4d29b3a4, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "q5_37.13f",     0x020000, 0xeb547ebc, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "q5_43.13h",     0x020000, 0x3ef65ea8, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "q5_34.10f",     0x020000, 0x7fcc1317, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "q5_40.10h",     0x020000, 0x7f14b7b4, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "q5_35.11f",     0x020000, 0xabacee26, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "q5_41.11h",     0x020000, 0xd3654067, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "q5_09.4b",      0x020000, 0x48496d80, ROMType::GRAPHICS, 0 },
    { "q5_01.4a",      0x020000, 0xc5453f56, ROMType::GRAPHICS, 0 },
    { "q5_13.9b",      0x020000, 0xc741ac52, ROMType::GRAPHICS, 0 },
    { "q5_05.9a",      0x020000, 0x143e068f, ROMType::GRAPHICS, 0 },
    { "q5_24.5e",      0x020000, 0xb419d139, ROMType::GRAPHICS, 0 },
    { "q5_17.5c",      0x020000, 0xbd3b4d11, ROMType::GRAPHICS, 0 },
    { "q5_38.8h",      0x020000, 0x9c24670c, ROMType::GRAPHICS, 0 },
    { "q5_32.8f",      0x020000, 0x3ef9c7c2, ROMType::GRAPHICS, 0 },
    { "q5_10.5b",      0x020000, 0x119e5e93, ROMType::GRAPHICS, 0 },
    { "q5_02.5a",      0x020000, 0xa2cadcbe, ROMType::GRAPHICS, 0 },
    { "q5_14.10b",     0x020000, 0xa8755f82, ROMType::GRAPHICS, 0 },
    { "q5_06.10a",     0x020000, 0xc92a91fc, ROMType::GRAPHICS, 0 },
    { "q5_25.7e",      0x020000, 0x979237cb, ROMType::GRAPHICS, 0 },
    { "q5_18.7c",      0x020000, 0xc57da03c, ROMType::GRAPHICS, 0 },
    { "q5_39.9h",      0x020000, 0xa5839b25, ROMType::GRAPHICS, 0 },
    { "q5_33.9f",      0x020000, 0x04d03930, ROMType::GRAPHICS, 0 },
    { "q5_23.13b",     0x010000, 0xe14dc524, ROMType::SOUND_PROGRAM, 0 },
    { "q5_30.12c",     0x020000, 0xd10c1b68, ROMType::SOUND_SAMPLE, 0 },
    { "q5_31.13c",     0x020000, 0x7d17e496, ROMType::SOUND_SAMPLE, 0 },
};

// Cadillacs and Dinosaurs
static const ROMEntry dino_roms[] = {
    { "cde_23a.8f",    0x080000, 0x8f4e585e, ROMType::PROGRAM, 0 },
    { "cde_22a.7f",    0x080000, 0x9278aa12, ROMType::PROGRAM, 0 },
    { "cde_21a.6f",    0x080000, 0x66d23de2, ROMType::PROGRAM, 0 },
    { "cd-1m.3a",      0x080000, 0x8da4f917, ROMType::GRAPHICS, 0 },
    { "cd-3m.5a",      0x080000, 0x6c40f603, ROMType::GRAPHICS, 0 },
    { "cd-2m.4a",      0x080000, 0x09c8fc2d, ROMType::GRAPHICS, 0 },
    { "cd-4m.6a",      0x080000, 0x637ff38f, ROMType::GRAPHICS, 0 },
    { "cd-5m.7a",      0x080000, 0x470befee, ROMType::GRAPHICS, 0 },
    { "cd-7m.9a",      0x080000, 0x22bfb7a3, ROMType::GRAPHICS, 0 },
    { "cd-6m.8a",      0x080000, 0xe7599ac4, ROMType::GRAPHICS, 0 },
    { "cd-8m.10a",     0x080000, 0x211b4b15, ROMType::GRAPHICS, 0 },
    { "cd_q.5k",       0x020000, 0x605fdb0b, ROMType::SOUND_PROGRAM, 0 },
    { "cd-q1.1k",      0x080000, 0x60927775, ROMType::SOUND_SAMPLE, 0 },
    { "cd-q2.2k",      0x080000, 0x770f4c47, ROMType::SOUND_SAMPLE, 0 },
    { "cd-q3.3k",      0x080000, 0x2f273ffc, ROMType::SOUND_SAMPLE, 0 },
    { "cd-q4.4k",      0x080000, 0x2c67821d, ROMType::SOUND_SAMPLE, 0 },
};

// Dynasty Wars
static const ROMEntry dynwar_roms[] = {
    { "30.11f",        0x020000, 0xf9ec6d68, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "35.11h",        0x020000, 0xe41fff2f, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "31.12f",        0x020000, 0xe3de76ff, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "36.12h",        0x020000, 0x7a13cfbf, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "tkm-9.8h",      0x080000, 0x93654bcf, ROMType::PROGRAM, 0 },
    { "tkm-5.7a",      0x080000, 0xf64bb6a0, ROMType::GRAPHICS, 0 },
    { "tkm-8.9a",      0x080000, 0x21fe6274, ROMType::GRAPHICS, 0 },
    { "tkm-6.3a",      0x080000, 0x0bf228cb, ROMType::GRAPHICS, 0 },
    { "tkm-7.5a",      0x080000, 0x1255dfb1, ROMType::GRAPHICS, 0 },
    { "tkm-1.8a",      0x080000, 0x44f7661e, ROMType::GRAPHICS, 0 },
    { "tkm-4.10a",     0x080000, 0xa54c515d, ROMType::GRAPHICS, 0 },
    { "tkm-2.4a",      0x080000, 0xca5c687c, ROMType::GRAPHICS, 0 },
    { "tkm-3.6a",      0x080000, 0xf9fe6591, ROMType::GRAPHICS, 0 },
    { "tke_17.12b",    0x010000, 0xb3b79d4f, ROMType::SOUND_PROGRAM, 0 },
    { "tke_18.11c",    0x020000, 0xac6e307d, ROMType::SOUND_SAMPLE, 0 },
    { "tke_19.12c",    0x020000, 0x068741db, ROMType::SOUND_SAMPLE, 0 },
};

// Forgotten Worlds
static const ROMEntry forgottn_roms[] = {
    { "lw40.12f",      0x020000, 0x73e920b7, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE }, /* Higher program numbers indicates a later revision */
    { "lw41.12h",      0x020000, 0x58210b9e, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE }, /* 1 byte difference: 0x66D4 == 0x0C versus 0x04 in lw15.12h below */
    { "lw42.13f",      0x020000, 0xbea45994, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "lw43.13h",      0x020000, 0x539b2339, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "lw-07.10g",     0x080000, 0xfd252a26, ROMType::PROGRAM, 0 }, 
    { "lw_2.2b",       0x020000, 0x4bd75fee, ROMType::GRAPHICS, 0 },
    { "lw_1.2a",       0x020000, 0x65f41485, ROMType::GRAPHICS, 0 },
    { "lw-08.9b",      0x080000, 0x25a8e43c, ROMType::GRAPHICS, 0 },
    { "lw_18.5e",      0x020000, 0xb4b6241b, ROMType::GRAPHICS, 0 },
    { "lw_17.5c",      0x020000, 0xc5eea115, ROMType::GRAPHICS, 0 },
    { "lw_30.8h",      0x020000, 0xb385954e, ROMType::GRAPHICS, 0 },
    { "lw_29.8f",      0x020000, 0x7bda1ac6, ROMType::GRAPHICS, 0 },
    { "lw_4.3b",       0x020000, 0x50cf757f, ROMType::GRAPHICS, 0 },
    { "lw_3.3a",       0x020000, 0xc03ef278, ROMType::GRAPHICS, 0 },
    { "lw_20.7e",      0x020000, 0xdf1a3665, ROMType::GRAPHICS, 0 },
    { "lw_19.7c",      0x020000, 0x15af8440, ROMType::GRAPHICS, 0 },
    { "lw_32.9h",      0x020000, 0x30967a15, ROMType::GRAPHICS, 0 },
    { "lw_31.9f",      0x020000, 0xc49d37fb, ROMType::GRAPHICS, 0 },
    { "lw-02.6b",      0x080000, 0x43e6c5c8, ROMType::GRAPHICS, 0 },
    { "lw_14.10b",     0x020000, 0x82862cce, ROMType::GRAPHICS, 0 },
    { "lw_13.10a",     0x020000, 0xb81c0e96, ROMType::GRAPHICS, 0 },
    { "lw-06.9d",      0x080000, 0x5b9edffc, ROMType::GRAPHICS, 0 },
    { "lw_26.10e",     0x020000, 0x57bcd032, ROMType::GRAPHICS, 0 },
    { "lw_25.10c",     0x020000, 0xbac91554, ROMType::GRAPHICS, 0 },	
    { "lw_16.11b",     0x020000, 0x40b26554, ROMType::GRAPHICS, 0 },
    { "lw_15.11a",     0x020000, 0x1b7d2e07, ROMType::GRAPHICS, 0 },
    { "lw_28.11e",     0x020000, 0xa805ad30, ROMType::GRAPHICS, 0 },
    { "lw_27.11c",     0x020000, 0x103c1bd2, ROMType::GRAPHICS, 0 },
    { "lw_37.13c",     0x010000, 0x59df2a63, ROMType::SOUND_PROGRAM, 0 },
    { "lw-03u.12e",    0x020000, 0x807d051f, ROMType::SOUND_SAMPLE, 0 },
    { "lw-04u.13e",    0x020000, 0xe6cd098e, ROMType::SOUND_SAMPLE, 0 },
};

// Ganbare! Marine Kun
static const ROMEntry ganbare_roms[] = {
    { "mrnj_23d.8f",   0x080000, 0xf929be72, ROMType::PROGRAM, 0 },
    { "mrnj_01.3a",    0x080000, 0x3f878020, ROMType::GRAPHICS, 0 },
    { "mrnj_02.4a",    0x080000, 0x3e5624d8, ROMType::GRAPHICS, 0 },
    { "mrnj_03.5a",    0x080000, 0xd1e61f96, ROMType::GRAPHICS, 0 },
    { "mrnj_04.6a",    0x080000, 0xd241971b, ROMType::GRAPHICS, 0 },
    { "mrnj_05.7a",    0x080000, 0xc0a14562, ROMType::GRAPHICS, 0 },
    { "mrnj_06.8a",    0x080000, 0xe6a71dfc, ROMType::GRAPHICS, 0 },
    { "mrnj_07.9a",    0x080000, 0x99afb6c7, ROMType::GRAPHICS, 0 },
    { "mrnj_08.10a",   0x080000, 0x52882c20, ROMType::GRAPHICS, 0 },
    { "mrnj_09.12a",   0x010000, 0x62470d72, ROMType::SOUND_PROGRAM, 0 },
    { "mrnj_18.11c",   0x020000, 0x08e13940, ROMType::SOUND_SAMPLE, 0 },
    { "mrnj_19.12c",   0x020000, 0x5fa59927, ROMType::SOUND_SAMPLE, 0 },
};

// Ghouls'n Ghosts
static const ROMEntry ghouls_roms[] = {
    { "dme_29.10h",    0x020000, 0x166a58a2, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "dme_30.10j",    0x020000, 0x7ac8407a, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "dme_27.9h",     0x020000, 0xf734b2be, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "dme_28.9j",     0x020000, 0x03d3e714, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "dm-17.7j",      0x080000, 0x3ea1b0f2, ROMType::PROGRAM, 0 },
    { "dm-05.3a",      0x080000, 0x0ba9c0b0, ROMType::GRAPHICS, 0 },
    { "dm-07.3f",      0x080000, 0x5d760ab9, ROMType::GRAPHICS, 0 },
    { "dm-06.3c",      0x080000, 0x4ba90b59, ROMType::GRAPHICS, 0 },
    { "dm-08.3g",      0x080000, 0x4bdee9de, ROMType::GRAPHICS, 0 },
    { "09.4a",         0x010000, 0xae24bb19, ROMType::GRAPHICS, 0 },
    { "18.7a",         0x010000, 0xd34e271a, ROMType::GRAPHICS, 0 },
    { "13.4e",         0x010000, 0x3f70dd37, ROMType::GRAPHICS, 0 },
    { "22.7e",         0x010000, 0x7e69e2e6, ROMType::GRAPHICS, 0 },
    { "11.4c",         0x010000, 0x37c9b6c6, ROMType::GRAPHICS, 0 },
    { "20.7c",         0x010000, 0x2f1345b4, ROMType::GRAPHICS, 0 },
    { "15.4g",         0x010000, 0x3c2a212a, ROMType::GRAPHICS, 0 },
    { "24.7g",         0x010000, 0x889aac05, ROMType::GRAPHICS, 0 },
    { "10.4b",         0x010000, 0xbcc0f28c, ROMType::GRAPHICS, 0 },
    { "19.7b",         0x010000, 0x2a40166a, ROMType::GRAPHICS, 0 },
    { "14.4f",         0x010000, 0x20f85c03, ROMType::GRAPHICS, 0 },
    { "23.7f",         0x010000, 0x8426144b, ROMType::GRAPHICS, 0 },
    { "12.4d",         0x010000, 0xda088d61, ROMType::GRAPHICS, 0 },
    { "21.7d",         0x010000, 0x17e11df0, ROMType::GRAPHICS, 0 },
    { "16.4h",         0x010000, 0xf187ba1c, ROMType::GRAPHICS, 0 },
    { "25.7h",         0x010000, 0x29f79c78, ROMType::GRAPHICS, 0 },
    { "26.10a",        0x010000, 0x3692f6e5, ROMType::SOUND_PROGRAM, 0 },
};

// Gulun.Pa!
static const ROMEntry gulunpa_roms[] = {
    { "26",		0x20000, 0xf30ffa29, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "30",		0x20000, 0x5d35f737, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "1",		0x80000, 0xb55e648f, ROMType::GRAPHICS, 0 },
    { "2",		0x80000, 0xad033bce, ROMType::GRAPHICS, 0 },
    { "3",		0x80000, 0x36c3951a, ROMType::GRAPHICS, 0 },
    { "4",		0x80000, 0xff0cb826, ROMType::GRAPHICS, 0 },
    { "9",		0x10000, 0x15afd06f, ROMType::SOUND_PROGRAM, 0 },
    { "18",		0x20000, 0x9997a34f, ROMType::SOUND_SAMPLE, 0 },
    { "19",		0x20000, 0xe95270ac, ROMType::SOUND_SAMPLE, 0 },
};

// Magical Pumpkin: Puroland de Daibouken
static const ROMEntry mpumpkin_roms[] = {
    { "mpa_23.8f",	0x080000, 0x38b9883a, ROMType::PROGRAM, 0 },
    { "mpa_01.3a",	0x080000, 0x7c8c0c22, ROMType::GRAPHICS, 0 },
    { "mpa_02.4a",	0x080000, 0x23f95339, ROMType::GRAPHICS, 0 },
    { "mpa_03.5a",	0x080000, 0x107842a6, ROMType::GRAPHICS, 0 },
    { "mpa_04.6a",	0x080000, 0xfce457ae, ROMType::GRAPHICS, 0 },
    { "mpa_05.7a",	0x080000, 0xba8f3585, ROMType::GRAPHICS, 0 },
    { "mpa_06.8a",	0x080000, 0x037f20cc, ROMType::GRAPHICS, 0 },
    { "mpa_07.9a",	0x080000, 0xba8f3585, ROMType::GRAPHICS, 0 },
    { "mpa_08.10a",	0x080000, 0x037f20cc, ROMType::GRAPHICS, 0 },
    { "mpa_10.3c",	0x080000, 0x870f3a2a, ROMType::GRAPHICS, 0 },
    { "mpa_11.4c",	0x080000, 0x8923fc3a, ROMType::GRAPHICS, 0 },
    { "mpa_12.5c",	0x080000, 0x87b88629, ROMType::GRAPHICS, 0 },
    { "mpa_13.6c",  0x080000, 0xa09a6acf, ROMType::GRAPHICS, 0 },
    { "mpa_09.12a",	0x010000, 0x0b5b1b72, ROMType::SOUND_PROGRAM, 0 },
    { "mpa_18.11c",	0x020000, 0xcef6d39e, ROMType::SOUND_SAMPLE, 0 },
    { "mpa_19.12c",	0x020000, 0x24947f8e, ROMType::SOUND_SAMPLE, 0 },
};

// Muscle Bomber Duo: Ultimate Team Battle
static const ROMEntry mbombrd_roms[] = {
    { "mbde_26.11e",   0x020000, 0x72b7451c, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "mbde_30.11f",   0x020000, 0xa036dc16, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "mbde_27.12e",   0x020000, 0x4086f534, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "mbde_31.12f",   0x020000, 0x085f47f0, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "mbde_24.9e",    0x020000, 0xc20895a5, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "mbde_28.9f",    0x020000, 0x2618d5e1, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "mbde_25.10e",   0x020000, 0x9bdb6b11, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "mbde_29.10f",   0x020000, 0x3f52d5e5, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "mbde_21.6f",    0x080000, 0x690c026a, ROMType::PROGRAM, 0 },
    { "mbde_20.5f",    0x080000, 0xb8b2139b, ROMType::PROGRAM, 0 },
    { "mb-1m.3a",      0x080000, 0x41468e06, ROMType::GRAPHICS, 0 },
    { "mb-3m.5a",  	   0x080000, 0xf453aa9e, ROMType::GRAPHICS, 0 },
    { "mb-2m.4a",      0x080000, 0x2ffbfea8, ROMType::GRAPHICS, 0 },
    { "mb-4m.6a",      0x080000, 0x1eb9841d, ROMType::GRAPHICS, 0 },
    { "mb-5m.7a",      0x080000, 0x506b9dc9, ROMType::GRAPHICS, 0 },
    { "mb-7m.9a",      0x080000, 0xaff8c2fb, ROMType::GRAPHICS, 0 },
    { "mb-6m.8a",      0x080000, 0xb76c70e9, ROMType::GRAPHICS, 0 },
    { "mb-8m.10a",     0x080000, 0xe60c9556, ROMType::GRAPHICS, 0 },
    { "mb-10m.3c",     0x080000, 0x97976ff5, ROMType::GRAPHICS, 0 },
    { "mb-12m.5c",     0x080000, 0xb350a840, ROMType::GRAPHICS, 0 },
    { "mb-11m.4c",     0x080000, 0x8fb94743, ROMType::GRAPHICS, 0 },
    { "mb-13m.6c",     0x080000, 0xda810d5f, ROMType::GRAPHICS, 0 },
    { "mb_q.5k",       0x020000, 0xd6fa76d1, ROMType::SOUND_PROGRAM, 0 },
    { "mb-q1.1k",      0x080000, 0x0630c3ce, ROMType::SOUND_SAMPLE, 0 },
    { "mb-q2.2k",      0x080000, 0x354f9c21, ROMType::SOUND_SAMPLE, 0 },
    { "mb-q3.3k",      0x080000, 0x7838487c, ROMType::SOUND_SAMPLE, 0 },
    { "mb-q4.4k",      0x080000, 0xab66e087, ROMType::SOUND_SAMPLE, 0 },
    { "mb-q5.1m",      0x080000, 0xc789fef2, ROMType::SOUND_SAMPLE, 0 },
    { "mb-q6.2m",      0x080000, 0xecb81b61, ROMType::SOUND_SAMPLE, 0 },
    { "mb-q7.3m",      0x080000, 0x041e49ba, ROMType::SOUND_SAMPLE, 0 },
    { "mb-q8.4m",      0x080000, 0x59fe702a, ROMType::SOUND_SAMPLE, 0 },
};

// Mega Twins
static const ROMEntry mtwins_roms[] = {
    { "che_30.11f",    0x020000, 0x9a2a2db1, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "che_35.11h",    0x020000, 0xa7f96b02, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "che_31.12f",    0x020000, 0xbbff8a99, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "che_36.12h",    0x020000, 0x0fa00c39, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "ck-32m.8h",     0x080000, 0x9b70bd41, ROMType::PROGRAM, 0 },
    { "ck-5m.7a",      0x080000, 0x4ec75f15, ROMType::GRAPHICS, 0 },
    { "ck-7m.9a",      0x080000, 0xd85d00d6, ROMType::GRAPHICS, 0 },
    { "ck-1m.3a",      0x080000, 0xf33ca9d4, ROMType::GRAPHICS, 0 },
    { "ck-3m.5a",      0x080000, 0x0ba2047f, ROMType::GRAPHICS, 0 },
    { "ch_09.12b",     0x010000, 0x4d4255b7, ROMType::SOUND_PROGRAM, 0 },
    { "ch_18.11c",     0x020000, 0xf909e8de, ROMType::SOUND_SAMPLE, 0 },
    { "ch_19.12c",     0x020000, 0xfc158cf7, ROMType::SOUND_SAMPLE, 0 },
};

// Pang! 3
static const ROMEntry pang3_roms[] = {
    { "pa3e_17a.11l",  0x080000, 0xa213fa80, ROMType::PROGRAM, 0 },
    { "pa3e_16a.10l",  0x080000, 0x7169ea67, ROMType::PROGRAM, 0 },
    { "pa3-01m.2c",    0x200000, 0x068a152c, ROMType::GRAPHICS, 0 },
    { "pa3-07m.2f",    0x200000, 0x3a4a619d, ROMType::GRAPHICS, 0 },
    { "pa3_11.11f",    0x020000, 0xcb1423a2, ROMType::SOUND_PROGRAM, 0 },
    { "pa3_05.10d",    0x020000, 0x73a10d5d, ROMType::SOUND_SAMPLE, 0 },
    { "pa3_06.11d",    0x020000, 0xaffa4f82, ROMType::SOUND_SAMPLE, 0 },
};

// Pnickies
static const ROMEntry pnickj_roms[] = {
    { "pnij_36.12f",   0x020000, 0x2d4ffb2b, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "pnij_42.12h",   0x020000, 0xc085dfaf, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "pnij_09.4b",    0x020000, 0x48177b0a, ROMType::GRAPHICS, 0 },
    { "pnij_01.4a",    0x020000, 0x01a0f311, ROMType::GRAPHICS, 0 },
    { "pnij_13.9b",    0x020000, 0x406451b0, ROMType::GRAPHICS, 0 },
    { "pnij_05.9a",    0x020000, 0x8c515dc0, ROMType::GRAPHICS, 0 },
    { "pnij_26.5e",    0x020000, 0xe2af981e, ROMType::GRAPHICS, 0 },
    { "pnij_18.5c",    0x020000, 0xf17a0e56, ROMType::GRAPHICS, 0 },
    { "pnij_38.8h",    0x020000, 0xeb75bd8c, ROMType::GRAPHICS, 0 },
    { "pnij_32.8f",    0x020000, 0x84560bef, ROMType::GRAPHICS, 0 },
    { "pnij_10.5b",    0x020000, 0xc2acc171, ROMType::GRAPHICS, 0 },
    { "pnij_02.5a",    0x020000, 0x0e21fc33, ROMType::GRAPHICS, 0 },
    { "pnij_14.10b",   0x020000, 0x7fe59b19, ROMType::GRAPHICS, 0 },
    { "pnij_06.10a",   0x020000, 0x79f4bfe3, ROMType::GRAPHICS, 0 },
    { "pnij_27.7e",    0x020000, 0x83d5cb0e, ROMType::GRAPHICS, 0 },
    { "pnij_19.7c",    0x020000, 0xaf08b230, ROMType::GRAPHICS, 0 },
    { "pnij_39.9h",    0x020000, 0x70fbe579, ROMType::GRAPHICS, 0 },
    { "pnij_33.9f",    0x020000, 0x3ed2c680, ROMType::GRAPHICS, 0 },
    { "pnij_17.13b",   0x010000, 0xe86f787a, ROMType::SOUND_PROGRAM, 0 },
    { "pnij_24.12c",   0x020000, 0x5092257d, ROMType::SOUND_SAMPLE, 0 },
    { "pnij_25.13c",   0x020000, 0x22109aaa, ROMType::SOUND_SAMPLE, 0 },
};

// Pokonyan! Balloon
static const ROMEntry pokonyan_roms[] = {
    { "xmqq-12f.bin",  0x020000, 0x196297bf, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "xmqq-12h.bin",  0x020000, 0x2d7ee2e9, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "xmqq-13f.bin",  0x020000, 0x8f6abf26, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "xmqq-13h.bin",  0x020000, 0x3fefe432, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "xmqq-4b.bin",   0x020000, 0x933ab76d, ROMType::GRAPHICS, 0 },
    { "xmqq-4a.bin",   0x020000, 0xb098e7a9, ROMType::GRAPHICS, 0 },
    { "xmqq-9b.bin",   0x020000, 0xb66d62d4, ROMType::GRAPHICS, 0 },
    { "xmqq-9a.bin",   0x020000, 0x9c23e40b, ROMType::GRAPHICS, 0 },
    { "xmqq-5e.bin",   0x020000, 0x63d06d6f, ROMType::GRAPHICS, 0 },
    { "xmqq-5c.bin",   0x020000, 0xe2169bb5, ROMType::GRAPHICS, 0 },
    { "xmqq-8h.bin",   0x020000, 0x113121f5, ROMType::GRAPHICS, 0 },
    { "xmqq-8f.bin",   0x020000, 0xbeb00e07, ROMType::GRAPHICS, 0 },
    { "xmqq-5b.bin",   0x020000, 0x05354905, ROMType::GRAPHICS, 0 },
    { "xmqq-5a.bin",   0x020000, 0xbd40215e, ROMType::GRAPHICS, 0 },
    { "xmqq-10b.bin",  0x020000, 0x9fa773ef, ROMType::GRAPHICS, 0 },
    { "xmqq-10a.bin",  0x020000, 0x638d4bc7, ROMType::GRAPHICS, 0 },
    { "xmqq-7e.bin",   0x020000, 0x72c45858, ROMType::GRAPHICS, 0 },
    { "xmqq-7c.bin",   0x020000, 0xd91cda18, ROMType::GRAPHICS, 0 },
    { "xmqq-9h.bin",   0x020000, 0x3cd8594b, ROMType::GRAPHICS, 0 },
    { "xmqq-9f.bin",   0x020000, 0x1ec10bed, ROMType::GRAPHICS, 0 },
    { "xmqq-13b.bin",  0x010000, 0x4e8b81a8, ROMType::SOUND_PROGRAM, 0 },
    { "xmqq-12c.bin",  0x020000, 0x71ac69ad, ROMType::SOUND_SAMPLE, 0 },
    { "xmqq-13c.bin",  0x020000, 0x71e29699, ROMType::SOUND_SAMPLE, 0 },
};

// Quiz & Dragons: Capcom Quiz Game
static const ROMEntry qad_roms[] = {
    { "qdu_36a.12f",   0x020000, 0xde9c24a0, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "qdu_42a.12h",   0x020000, 0xcfe36f0c, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "qdu_37a.13f",   0x020000, 0x10d22320, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "qdu_43a.13h",   0x020000, 0x15e6beb9, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "qd_09.4b",      0x020000, 0x8c3f9f44, ROMType::GRAPHICS, 0 },
    { "qd_01.4a",      0x020000, 0xf688cf8f, ROMType::GRAPHICS, 0 },
    { "qd_13.9b",      0x020000, 0xafbd551b, ROMType::GRAPHICS, 0 },
    { "qd_05.9a",      0x020000, 0xc3db0910, ROMType::GRAPHICS, 0 },
    { "qd_24.5e",      0x020000, 0x2f1bd0ec, ROMType::GRAPHICS, 0 },
    { "qd_17.5c",      0x020000, 0xa812f9e2, ROMType::GRAPHICS, 0 },
    { "qd_38.8h",      0x020000, 0xccdddd1f, ROMType::GRAPHICS, 0 },
    { "qd_32.8f",      0x020000, 0xa8d295d3, ROMType::GRAPHICS, 0 },
    { "qd_23.13b",     0x010000, 0xcfb5264b, ROMType::SOUND_PROGRAM, 0 },
    { "qdu_30.12c",    0x020000, 0xf190da84, ROMType::SOUND_SAMPLE, 0 },
    { "qdu_31.13c",    0x020000, 0xb7583f73, ROMType::SOUND_SAMPLE, 0 },
};

// Quiz Tonosama no Yabou 2: Zenkoku-ban
static const ROMEntry qtono2j_roms[] = {
    { "tn2j_30.11e",   0x020000, 0x9226eb5e, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "tn2j_37.11f",   0x020000, 0xd1d30da1, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "tn2j_31.12e",   0x020000, 0x015e6a8a, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "tn2j_38.12f",   0x020000, 0x1f139bcc, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "tn2j_28.9e",    0x020000, 0x86d27f71, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "tn2j_35.9f",    0x020000, 0x7a1ab87d, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "tn2j_29.10e",   0x020000, 0x9c384e99, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "tn2j_36.10f",   0x020000, 0x4c4b2a0a, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "tn2-02m.4a",    0x080000, 0xf2016a34, ROMType::GRAPHICS, 0 },
    { "tn2-04m.6a",    0x080000, 0x094e0fb1, ROMType::GRAPHICS, 0 },
    { "tn2-01m.3a",    0x080000, 0xcb950cf9, ROMType::GRAPHICS, 0 },
    { "tn2-03m.5a",    0x080000, 0x18a5bf59, ROMType::GRAPHICS, 0 },
    { "tn2-11m.4c",    0x080000, 0xd0edd30b, ROMType::GRAPHICS, 0 },
    { "tn2-13m.6c",    0x080000, 0x426621c3, ROMType::GRAPHICS, 0 },
    { "tn2-10m.3c",    0x080000, 0xa34ece70, ROMType::GRAPHICS, 0 },
    { "tn2-12m.5c",    0x080000, 0xe04ff2f4, ROMType::GRAPHICS, 0 },
    { "tn2j_09.12a",   0x010000, 0xe464b969, ROMType::SOUND_PROGRAM, 0 },
    { "tn2j_18.11c",   0x020000, 0xa40bf9a7, ROMType::SOUND_SAMPLE, 0 },
    { "tn2j_19.12c",   0x020000, 0x5b3b931e, ROMType::SOUND_SAMPLE, 0 },
};

// Street Fighter Zero (CPS Changer, Japan 951020)
static const ROMEntry sfzch_roms[] = {
    { "sfzch23",       0x080000, 0x1140743f, ROMType::PROGRAM, 0 },
    { "sfza22",        0x080000, 0x8d9b2480, ROMType::PROGRAM, 0 },
    { "sfzch21",       0x080000, 0x5435225d, ROMType::PROGRAM, 0 },
    { "sfza20",        0x080000, 0x806e8f38, ROMType::PROGRAM, 0 },
    { "sfz_01.3a",     0x080000, 0x0dd53e62, ROMType::GRAPHICS, 0 },
    { "sfz_02.4a",     0x080000, 0x94c31e3f, ROMType::GRAPHICS, 0 },
    { "sfz_03.5a",     0x080000, 0x9584ac85, ROMType::GRAPHICS, 0 },
    { "sfz_04.6a",     0x080000, 0xb983624c, ROMType::GRAPHICS, 0 },
    { "sfz_05.7a",     0x080000, 0x2b47b645, ROMType::GRAPHICS, 0 },
    { "sfz_06.8a",     0x080000, 0x74fd9fb1, ROMType::GRAPHICS, 0 },
    { "sfz_07.9a",     0x080000, 0xbb2c734d, ROMType::GRAPHICS, 0 },
    { "sfz_08.10a",    0x080000, 0x454f7868, ROMType::GRAPHICS, 0 },
    { "sfz_10.3c",     0x080000, 0x2a7d675e, ROMType::GRAPHICS, 0 },
    { "sfz_11.4c",     0x080000, 0xe35546c8, ROMType::GRAPHICS, 0 },
    { "sfz_12.5c",     0x080000, 0xf122693a, ROMType::GRAPHICS, 0 },
    { "sfz_13.6c",     0x080000, 0x7cf942c8, ROMType::GRAPHICS, 0 },
    { "sfz_14.7c",     0x080000, 0x09038c81, ROMType::GRAPHICS, 0 },
    { "sfz_15.8c",     0x080000, 0x1aa17391, ROMType::GRAPHICS, 0 },
    { "sfz_16.9c",     0x080000, 0x19a5abd6, ROMType::GRAPHICS, 0 },
    { "sfz_17.10c",    0x080000, 0x248b3b73, ROMType::GRAPHICS, 0 },
    { "sfz_09.12a",    0x010000, 0xc772628b, ROMType::SOUND_PROGRAM, 0 },
    { "sfz_18.11c",    0x020000, 0x61022b2d, ROMType::SOUND_SAMPLE, 0 },
    { "sfz_19.12c",    0x020000, 0x3b5886d5, ROMType::SOUND_SAMPLE, 0 },
};

// Saturday Night Slam Masters
static const ROMEntry slammast_roms[] = {
    { "mbe_23e.8f",    	0x080000, 0x5394057a, ROMType::PROGRAM, 0 },
    { "mbe_24b.9e",    	0x020000, 0x95d5e729, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "mbe_28b.9f",    	0x020000, 0xb1c7cbcb, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "mbe_25b.10e",   	0x020000, 0xa50d3fd4, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },	
    { "mbe_29b.10f",   	0x020000, 0x08e32e56, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "mbe_21a.6f",    	0x080000, 0xd5007b05, ROMType::PROGRAM, 0 },
    { "mbe_20a.5f",    	0x080000, 0xaeb557b0, ROMType::PROGRAM, 0 },
    { "mb-1m.3a",      	0x080000, 0x41468e06, ROMType::GRAPHICS, 0 },
    { "mb-3m.5a",      	0x080000, 0xf453aa9e, ROMType::GRAPHICS, 0 },
    { "mb-2m.4a",      	0x080000, 0x2ffbfea8, ROMType::GRAPHICS, 0 },
    { "mb-4m.6a",      	0x080000, 0x1eb9841d, ROMType::GRAPHICS, 0 },
    { "mb-5m.7a",      	0x080000, 0x506b9dc9, ROMType::GRAPHICS, 0 },
    { "mb-7m.9a",      	0x080000, 0xaff8c2fb, ROMType::GRAPHICS, 0 },
    { "mb-6m.8a",      	0x080000, 0xb76c70e9, ROMType::GRAPHICS, 0 },
    { "mb-8m.10a",     	0x080000, 0xe60c9556, ROMType::GRAPHICS, 0 },
    { "mb-10m.3c",     	0x080000, 0x97976ff5, ROMType::GRAPHICS, 0 },
    { "mb-12m.5c",     	0x080000, 0xb350a840, ROMType::GRAPHICS, 0 },
    { "mb-11m.4c",     	0x080000, 0x8fb94743, ROMType::GRAPHICS, 0 },
    { "mb-13m.6c",     	0x080000, 0xda810d5f, ROMType::GRAPHICS, 0 },
    { "mb_qa.5k",      	0x020000, 0xe21a03c4, ROMType::SOUND_PROGRAM, 0 },
    { "mb-q1.1k",     	0x080000, 0x0630c3ce, ROMType::SOUND_SAMPLE, 0 },
    { "mb-q2.2k",     	0x080000, 0x354f9c21, ROMType::SOUND_SAMPLE, 0 },
    { "mb-q3.3k",     	0x080000, 0x7838487c, ROMType::SOUND_SAMPLE, 0 },
    { "mb-q4.4k",     	0x080000, 0xab66e087, ROMType::SOUND_SAMPLE, 0 },
    { "mb-q5.1m",     	0x080000, 0xc789fef2, ROMType::SOUND_SAMPLE, 0 },
    { "mb-q6.2m",     	0x080000, 0xecb81b61, ROMType::SOUND_SAMPLE, 0 },
    { "mb-q7.3m",     	0x080000, 0x041e49ba, ROMType::SOUND_SAMPLE, 0 },
    { "mb-q8.4m",     	0x080000, 0x59fe702a, ROMType::SOUND_SAMPLE, 0 },
};

// Strider
static const ROMEntry strider_roms[] = {
    { "30.11f",        0x020000, 0xda997474, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "35.11h",        0x020000, 0x5463aaa3, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "31.12f",        0x020000, 0xd20786db, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "36.12h",        0x020000, 0x21aa2863, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "st-14.8h",      0x080000, 0x9b3cfc08, ROMType::PROGRAM, 0 },
    { "st-2.8a",       0x080000, 0x4eee9aea, ROMType::GRAPHICS, 0 },
    { "st-11.10a",     0x080000, 0x2d7f21e4, ROMType::GRAPHICS, 0 },
    { "st-5.4a",       0x080000, 0x7705aa46, ROMType::GRAPHICS, 0 },
    { "st-9.6a",       0x080000, 0x5b18b722, ROMType::GRAPHICS, 0 },
    { "st-1.7a",       0x080000, 0x005f000b, ROMType::GRAPHICS, 0 },
    { "st-10.9a",      0x080000, 0xb9441519, ROMType::GRAPHICS, 0 },
    { "st-4.3a",       0x080000, 0xb7d04e8b, ROMType::GRAPHICS, 0 },
    { "st-8.5a",       0x080000, 0x6b4713b4, ROMType::GRAPHICS, 0 },
    { "09.12b",        0x010000, 0x2ed403bc, ROMType::SOUND_PROGRAM, 0 },
    { "18.11c",        0x020000, 0x4386bc80, ROMType::SOUND_SAMPLE, 0 },
    { "19.12c",        0x020000, 0x444536d7, ROMType::SOUND_SAMPLE, 0 },
};

// U.N. Squadron
static const ROMEntry unsquad_roms[] = {
    { "aru_30.11f",    0x020000, 0x24d8f88d, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "aru_35.11h",    0x020000, 0x8b954b59, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "aru_31.12f",    0x020000, 0x33e9694b, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "aru_36.12h",    0x020000, 0x7cc8fb9e, ROMType::PROGRAM, ROM_FLAG_INTERLEAVE },
    { "ar-32m.8h",     0x080000, 0xae1d7fb0, ROMType::PROGRAM, 0 },
    { "ar-5m.7a",      0x080000, 0xbf4575d8, ROMType::GRAPHICS, 0 },
    { "ar-7m.9a",      0x080000, 0xa02945f4, ROMType::GRAPHICS, 0 },
    { "ar-1m.3a",      0x080000, 0x5965ca8d, ROMType::GRAPHICS, 0 },
    { "ar-3m.5a",      0x080000, 0xac6db17d, ROMType::GRAPHICS, 0 },
    { "ar_09.12b",     0x010000, 0xf3dd1367, ROMType::SOUND_PROGRAM, 0 },
    { "aru_18.11c",    0x020000, 0x584b43a9, ROMType::SOUND_SAMPLE, 0 },
};

// CPS1 decryption keys
static const CPS1DecryptKeys wof_decKeys = { 0x01234567, 0x54163072, 0x5151, 0x51 };
static const CPS1DecryptKeys dino_decKeys = { 0x76543210, 0x24601357, 0x4343, 0x43 };
static const CPS1DecryptKeys punisher_decKeys = { 0x67452103, 0x75316024, 0x2222, 0x22 };
static const CPS1DecryptKeys slammast_decKeys = { 0x54321076, 0x65432107, 0x3131, 0x19 };

// Game database
const GameInfo GameDatabase::s_cps1_games[] = {
    {
        "1941", "1941: Counter Attack", 1, game1941_roms, static_cast<u32>(sizeof(game1941_roms) / sizeof(game1941_roms[0])),
        GameFlags::GAME_FLAG_VERTICAL_SCREEN, CPSBoard::CPS_B_05, CPSMapper::MAPPER_YI24B,
    },
    {
        "3wonders", "Three Wonders", 1, threewonders_roms, static_cast<u32>(sizeof(threewonders_roms) / sizeof(threewonders_roms[0])),
        0, CPSBoard::CPS_B_21_BT1, CPSMapper::MAPPER_RT24B,
    },
    {
        "area88", "Area 88", 1, area88_roms, static_cast<u32>(sizeof(area88_roms) / sizeof(area88_roms[0])),
        0, CPSBoard::CPS_B_11, CPSMapper::MAPPER_AR22B,
    },
    {
        "captcomm", "Captain Commando", 1, captcomm_roms, static_cast<u32>(sizeof(captcomm_roms) / sizeof(captcomm_roms[0])),
        0, CPSBoard::CPS_B_21_BT3, CPSMapper::MAPPER_CC63B,
    },
    {
        "cawing", "Carrier Air Wing", 1, cawing_roms, static_cast<u32>(sizeof(cawing_roms) / sizeof(cawing_roms[0])),
        0, CPSBoard::CPS_B_16, CPSMapper::MAPPER_CA24B,
    },
    {
        "cworld2j", "Adventure Quiz Capcom World 2", 1, cworld2j_roms, static_cast<u32>(sizeof(cworld2j_roms) / sizeof(cworld2j_roms[0])),
        0, CPSBoard::CPS_B_21_BT6, CPSMapper::MAPPER_Q522B,
    },
    {
        "dino", "Cadillacs and Dinosaurs", 1, dino_roms, static_cast<u32>(sizeof(dino_roms) / sizeof(dino_roms[0])),
        GameFlags::GAME_FLAG_CPS1_QSOUND, CPSBoard::CPS_B_21_QS2, CPSMapper::MAPPER_CD63B, &dino_decKeys,
    },
    {
        "dynwar", "Dynasty Wars", 1, dynwar_roms, static_cast<u32>(sizeof(dynwar_roms) / sizeof(dynwar_roms[0])),
        0, CPSBoard::CPS_B_02, CPSMapper::MAPPER_TK22B,
    },
    {
        "ffight", "Final Fight", 1, ffight_roms, static_cast<u32>(sizeof(ffight_roms) / sizeof(ffight_roms[0])),
        0, CPSBoard::CPS_B_04, CPSMapper::MAPPER_S224B,
    },
    {
        "forgottn", "Forgotten Worlds", 1, forgottn_roms, static_cast<u32>(sizeof(forgottn_roms) / sizeof(forgottn_roms[0])),
        0, CPSBoard::CPS_B_01, CPSMapper::MAPPER_LW621,
    },
    {
        "ganbare", "Ganbare! Marine Kun", 1, ganbare_roms, static_cast<u32>(sizeof(ganbare_roms) / sizeof(ganbare_roms[0])),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_SFZCH,
    },
    {
        "ghouls", "Ghouls'n Ghosts", 1, ghouls_roms, static_cast<u32>(sizeof(ghouls_roms) / sizeof(ghouls_roms[0])),
        0, CPSBoard::CPS_B_01, CPSMapper::MAPPER_DM620,
    },
    {
        "gulunpa", "Gulun.Pa!", 1, gulunpa_roms, static_cast<u32>(sizeof(gulunpa_roms) / sizeof(gulunpa_roms[0])),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_GULUN,
    },
    {
        "knights", "Knights of the Round", 1, knights_roms, static_cast<u32>(sizeof(knights_roms) / sizeof(knights_roms[0])),
        0, CPSBoard::CPS_B_21_BT4, CPSMapper::MAPPER_KR63B,
    },
    {
        "kod", "The King of Dragons", 1, kod_roms, static_cast<u32>(sizeof(kod_roms) / sizeof(kod_roms[0])),
        0, CPSBoard::CPS_B_21_BT2, CPSMapper::MAPPER_KD29B,
    },
    {
        "mbombrd", "Muscle Bomber Duo: Ultimate Team Battle", 1, mbombrd_roms, static_cast<u32>(sizeof(mbombrd_roms) / sizeof(mbombrd_roms[0])),
        GameFlags::GAME_FLAG_CPS1_QSOUND, CPSBoard::CPS_B_21_QS5, CPSMapper::MAPPER_MB63B,
    },
    {
        "megaman", "Mega Man: The Power Battle", 1, megaman_roms, static_cast<u32>(sizeof(megaman_roms) / sizeof(megaman_roms[0])),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_RCM63B,
    },
    {
        "mercs", "Mercs", 1, mercs_roms, static_cast<u32>(sizeof(mercs_roms) / sizeof(mercs_roms[0])),
        GameFlags::GAME_FLAG_VERTICAL_SCREEN, CPSBoard::CPS_B_12, CPSMapper::MAPPER_O224B,
    },
    {
        "mpumpkin", "Magical Pumpkin: Puroland de Daibouken", 1, mpumpkin_roms, static_cast<u32>(sizeof(mpumpkin_roms) / sizeof(mpumpkin_roms[0])),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_SFZ63B,
    },
    {
        "msword", "Magic Sword", 1, msword_roms, static_cast<u32>(sizeof(msword_roms) / sizeof(msword_roms[0])),
        0, CPSBoard::CPS_B_13, CPSMapper::MAPPER_MS24B,
    },
    {
        "mtwins", "Mega Twins", 1, mtwins_roms, static_cast<u32>(sizeof(mtwins_roms) / sizeof(mtwins_roms[0])),
        0, CPSBoard::CPS_B_14, CPSMapper::MAPPER_CK24B,
    },
    {
        "nemo", "Nemo", 1, nemo_roms, static_cast<u32>(sizeof(nemo_roms) / sizeof(nemo_roms[0])),
        0, CPSBoard::CPS_B_15, CPSMapper::MAPPER_NM24B,
    },
    {
        "pang3", "Pang! 3", 1, pang3_roms, static_cast<u32>(sizeof(pang3_roms) / sizeof(pang3_roms[0])),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_CP1B1F,
    },
    {
        "pnickj", "Pnickies", 1, pnickj_roms, static_cast<u32>(sizeof(pnickj_roms) / sizeof(pnickj_roms[0])),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_PKB10B,
    },
    {
        "pokonyan", "Pokonyan! Balloon", 1, pokonyan_roms, static_cast<u32>(sizeof(pokonyan_roms) / sizeof(pokonyan_roms[0])),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_POKONYAN,
    },
    {
        "punisher", "The Punisher", 1, punisher_roms, static_cast<u32>(sizeof(punisher_roms) / sizeof(punisher_roms[0])),
        GameFlags::GAME_FLAG_CPS1_QSOUND, CPSBoard::CPS_B_21_QS3, CPSMapper::MAPPER_PS63B, &punisher_decKeys,
    },
    {
        "qad", "Quiz & Dragons: Capcom Quiz Game", 1, qad_roms, static_cast<u32>(sizeof(qad_roms) / sizeof(qad_roms[0])),
        0, CPSBoard::CPS_B_21_BT7, CPSMapper::MAPPER_QD22B,
    },
    {
        "qtono2j", "Quiz Tonosama no Yabou 2: Zenkoku-ban", 1, qtono2j_roms, static_cast<u32>(sizeof(qtono2j_roms) / sizeof(qtono2j_roms[0])),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_TN2292,
    },
    {
        "sf2", "Street Fighter II: The World Warrior", 1, sf2_roms, static_cast<u32>(sizeof(sf2_roms) / sizeof(sf2_roms[0])),
        0, CPSBoard::CPS_B_11, CPSMapper::MAPPER_STF29
    },
    {
        "sf2ce", "Street Fighter II: Champion Edition", 1, sf2ce_roms, static_cast<u32>(sizeof(sf2ce_roms) / sizeof(sf2ce_roms[0])),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_S9263B
    },
    {
        "sf2hf", "Street Fighter II: Hyper Fighting", 1, sf2hf_roms, static_cast<u32>(sizeof(sf2hf_roms) / sizeof(sf2hf_roms[0])),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_S9263B,
    },
    {
        "sfzch", "Street Fighter Zero (CPS Changer, Japan 951020)", 1, sfzch_roms, static_cast<u32>(sizeof(sfzch_roms) / sizeof(sfzch_roms[0])),
        0, CPSBoard::CPS_B_21_DEF, CPSMapper::MAPPER_SFZ63B,
    },
    {
        "slammast", "Saturday Night Slam Masters", 1, slammast_roms, static_cast<u32>(sizeof(slammast_roms) / sizeof(slammast_roms[0])),
        GameFlags::GAME_FLAG_CPS1_QSOUND, CPSBoard::CPS_B_21_QS4, CPSMapper::MAPPER_MB63B, &slammast_decKeys,
    },
    {
        "strider", "Strider", 1, strider_roms, static_cast<u32>(sizeof(strider_roms) / sizeof(strider_roms[0])),
        0, CPSBoard::CPS_B_01, CPSMapper::MAPPER_ST24M1,
    },
    {
        "unsquad", "U.N. Squadron", 1, unsquad_roms, static_cast<u32>(sizeof(unsquad_roms) / sizeof(unsquad_roms[0])),
        0, CPSBoard::CPS_B_11, CPSMapper::MAPPER_AR24B,
    },
    {
        "varth", "Varth: Operation Thunderstorm", 1, varth_roms, static_cast<u32>(sizeof(varth_roms) / sizeof(varth_roms[0])),
        GameFlags::GAME_FLAG_VERTICAL_SCREEN, CPSBoard::CPS_B_04, CPSMapper::MAPPER_VA63B,
    },
    {
        "willow", "Willow", 1, willow_roms, static_cast<u32>(sizeof(willow_roms) / sizeof(willow_roms[0])),
        0, CPSBoard::CPS_B_03, CPSMapper::MAPPER_WL24B,
    },
    {
        "wof", "Warriors of Fate", 1, wof_roms, static_cast<u32>(sizeof(wof_roms) / sizeof(wof_roms[0])),
        GameFlags::GAME_FLAG_CPS1_QSOUND, CPSBoard::CPS_B_21_QS1, CPSMapper::MAPPER_TK263B, &wof_decKeys,
    },
};

const u32 GameDatabase::s_cps1_gameCount = static_cast<u32>(sizeof(s_cps1_games) / sizeof(s_cps1_games[0]));

} // namespace cps
