#include "decrypt.h"
#include "../../types.h"
#include <cstring>

namespace cps {

// Helper macros
#define BIT(x, n) (((x) >> (n)) & 1)

// ========================================================
// CPS2 decryption functions
// ========================================================

// Feistel network bit groups
static const s32 fn1_groupA[8] = { 10, 4, 6, 7, 2, 13, 15, 14 };
static const s32 fn1_groupB[8] = {  0, 1, 3, 5, 8,  9, 11, 12 };

static const s32 fn2_groupA[8] = { 6, 0, 2, 13, 1,  4, 14,  7 };
static const s32 fn2_groupB[8] = { 3, 5, 9, 10, 8, 15, 12, 11 };

// S-box structures
struct SBox {
    const u8 table[64];
    const s32 inputs[6];    // positions of input bits, -1 means no input except from key
    const s32 outputs[2];   // positions of output bits
};

struct OptimisedSBox {
    u8 input_lookup[256];
    u8 output[64];
};

// Include all s-box definitions (from FBNeo)
#include "decrypt_sboxes.inc"

// Extract inputs from value based on input bit positions
static s32 extract_inputs(u32 val, const s32* inputs) {
    s32 res = 0;
    for (s32 i = 0; i < 6; ++i) {
        if (inputs[i] != -1) {
            res |= BIT(val, inputs[i]) << i;
        }
    }
    return res;
}

// Optimize s-boxes for fast lookup
static void optimise_sboxes(OptimisedSBox* out, const SBox* in) {
    for (s32 box = 0; box < 4; ++box) {
        // Precalculate input lookup
        for (s32 i = 0; i < 256; ++i) {
            out[box].input_lookup[i] = static_cast<u8>(extract_inputs(i, in[box].inputs));
        }
        
        // Precalculate output masks
        for (s32 i = 0; i < 64; ++i) {
            u8 o = in[box].table[i];
            out[box].output[i] = 0;
            if (o & 1) {
                out[box].output[i] |= 1 << in[box].outputs[0];
            }
            if (o & 2) {
                out[box].output[i] |= 1 << in[box].outputs[1];
            }
        }
    }
}

// F function using s-boxes
static u8 fn(u8 in, const OptimisedSBox* sboxes, u32 key) {
    const OptimisedSBox* sbox1 = &sboxes[0];
    const OptimisedSBox* sbox2 = &sboxes[1];
    const OptimisedSBox* sbox3 = &sboxes[2];
    const OptimisedSBox* sbox4 = &sboxes[3];
    
    return sbox1->output[sbox1->input_lookup[in] ^ ((key >>  0) & 0x3f)] |
           sbox2->output[sbox2->input_lookup[in] ^ ((key >>  6) & 0x3f)] |
           sbox3->output[sbox3->input_lookup[in] ^ ((key >> 12) & 0x3f)] |
           sbox4->output[sbox4->input_lookup[in] ^ ((key >> 18) & 0x3f)];
}

// Expand 64-bit master key to 96-bit key for 1st Feistel network
static void expand_1st_key(u32* dstkey, const u32* srckey) {
    static const s32 bits[96] = {
        33, 58, 49, 36,  0, 31,
        22, 30,  3, 16,  5, 53,
        10, 41, 23, 19, 27, 39,
        43,  6, 34, 12, 61, 21,
        48, 13, 32, 35,  6, 42,
        43, 14, 21, 41, 52, 25,
        18, 47, 46, 37, 57, 53,
        20,  8, 55, 54, 59, 60,
        27, 33, 35, 18,  8, 15,
        63,  1, 50, 44, 16, 46,
         5,  4, 45, 51, 38, 25,
        13, 11, 62, 29, 48,  2,
        59, 61, 62, 56, 51, 57,
        54,  9, 24, 63, 22,  7,
        26, 42, 45, 40, 23, 14,
         2, 31, 52, 28, 44, 17,
    };
    
    dstkey[0] = 0;
    dstkey[1] = 0;
    dstkey[2] = 0;
    dstkey[3] = 0;
    
    for (s32 i = 0; i < 96; ++i) {
        dstkey[i / 24] |= BIT(srckey[bits[i] / 32], bits[i] % 32) << (i % 24);
    }
}

// Expand 64-bit key to 96-bit key for 2nd Feistel network
static void expand_2nd_key(u32* dstkey, const u32* srckey) {
    static const s32 bits[96] = {
        34,  9, 32, 24, 44, 54,
        38, 61, 47, 13, 28,  7,
        29, 58, 18,  1, 20, 60,
        15,  6, 11, 43, 39, 19,
        63, 23, 16, 62, 54, 40,
        31,  3, 56, 61, 17, 25,
        47, 38, 55, 57,  5,  4,
        15, 42, 22,  7,  2, 19,
        46, 37, 29, 39, 12, 30,
        49, 57, 31, 41, 26, 27,
        24, 36, 11, 63, 33, 16,
        56, 62, 48, 60, 59, 32,
        12, 30, 53, 48, 10,  0,
        50, 35,  3, 59, 14, 49,
        51, 45, 44,  2, 21, 33,
        55, 52, 23, 28,  8, 26,
    };
    
    dstkey[0] = 0;
    dstkey[1] = 0;
    dstkey[2] = 0;
    dstkey[3] = 0;
    
    for (s32 i = 0; i < 96; ++i) {
        dstkey[i / 24] |= BIT(srckey[bits[i] / 32], bits[i] % 32) << (i % 24);
    }
}

// Expand 16-bit seed to 64-bit subkey
static void expand_subkey(u32* subkey, u16 seed) {
    static const s32 bits[64] = {
         5, 10, 14,  9,  4,  0, 15,  6,  1,  8,  3,  2, 12,  7, 13, 11,
         5, 12,  7,  2, 13, 11,  9, 14,  4,  1,  6, 10,  8,  0, 15,  3,
         4, 10,  2,  0,  6,  9, 12,  1, 11,  7, 15,  8, 13,  5, 14,  3,
        14, 11, 12,  7,  4,  5,  2, 10,  1, 15,  0,  9,  8,  6, 13,  3,
    };
    
    subkey[0] = 0;
    subkey[1] = 0;
    
    for (s32 i = 0; i < 64; ++i) {
        subkey[i / 32] |= BIT(seed, bits[i]) << (i % 32);
    }
}

// Feistel network function
static u16 feistel(u16 val, const s32* bitsA, const s32* bitsB,
                   const OptimisedSBox* boxes1, const OptimisedSBox* boxes2,
                   const OptimisedSBox* boxes3, const OptimisedSBox* boxes4,
                   u32 key1, u32 key2, u32 key3, u32 key4) {
    // Extract left and right halves using bit positions
    // bitsB selects bits from val for left half (l), bitsA for right half (r)
    u8 l = static_cast<u8>(
        (BIT(val, bitsB[0]) << 0) |
        (BIT(val, bitsB[1]) << 1) |
        (BIT(val, bitsB[2]) << 2) |
        (BIT(val, bitsB[3]) << 3) |
        (BIT(val, bitsB[4]) << 4) |
        (BIT(val, bitsB[5]) << 5) |
        (BIT(val, bitsB[6]) << 6) |
        (BIT(val, bitsB[7]) << 7)
    );
    u8 r = static_cast<u8>(
        (BIT(val, bitsA[0]) << 0) |
        (BIT(val, bitsA[1]) << 1) |
        (BIT(val, bitsA[2]) << 2) |
        (BIT(val, bitsA[3]) << 3) |
        (BIT(val, bitsA[4]) << 4) |
        (BIT(val, bitsA[5]) << 5) |
        (BIT(val, bitsA[6]) << 6) |
        (BIT(val, bitsA[7]) << 7)
    );
    
    l ^= fn(r, boxes1, key1);
    r ^= fn(l, boxes2, key2);
    l ^= fn(r, boxes3, key3);
    r ^= fn(l, boxes4, key4);
    
    return (BIT(l, 0) << bitsA[0]) |
           (BIT(l, 1) << bitsA[1]) |
           (BIT(l, 2) << bitsA[2]) |
           (BIT(l, 3) << bitsA[3]) |
           (BIT(l, 4) << bitsA[4]) |
           (BIT(l, 5) << bitsA[5]) |
           (BIT(l, 6) << bitsA[6]) |
           (BIT(l, 7) << bitsA[7]) |
           (BIT(r, 0) << bitsB[0]) |
           (BIT(r, 1) << bitsB[1]) |
           (BIT(r, 2) << bitsB[2]) |
           (BIT(r, 3) << bitsB[3]) |
           (BIT(r, 4) << bitsB[4]) |
           (BIT(r, 5) << bitsB[5]) |
           (BIT(r, 6) << bitsB[6]) |
           (BIT(r, 7) << bitsB[7]);
}

// Main decryption function
// master_key: 64-bit encryption key (2x32-bit values)
// lower_limit: Lower address limit (in 16-bit words)
// upper_limit: Upper address limit (in 16-bit words)
// rom: Pointer to encrypted ROM data (bytes, big-endian 16-bit words)
// dec: Pointer to output buffer for decrypted ROM (bytes, big-endian 16-bit words)
// length: Length of ROM in bytes
void decryptCPS2(const u32* master_key, u32 lower_limit, u32 upper_limit,
                 const u8* rom, u8* dec, u32 length) {
    // Convert byte length to word length
    u32 wordLength = length / 2;
    if (wordLength == 0) return;
    
    // Convert byte pointers to word pointers for easier access
    // Note: ROM is stored as bytes but represents 16-bit big-endian words
    // We need to read/write as words, handling endianness
    
    u32 key1[4];
    OptimisedSBox sboxes1[4 * 4];
    OptimisedSBox sboxes2[4 * 4];
    
    // Optimize s-boxes (one-time setup)
    optimise_sboxes(&sboxes1[0 * 4], fn1_r1_boxes);
    optimise_sboxes(&sboxes1[1 * 4], fn1_r2_boxes);
    optimise_sboxes(&sboxes1[2 * 4], fn1_r3_boxes);
    optimise_sboxes(&sboxes1[3 * 4], fn1_r4_boxes);
    optimise_sboxes(&sboxes2[0 * 4], fn2_r1_boxes);
    optimise_sboxes(&sboxes2[1 * 4], fn2_r2_boxes);
    optimise_sboxes(&sboxes2[2 * 4], fn2_r3_boxes);
    optimise_sboxes(&sboxes2[3 * 4], fn2_r4_boxes);
    
    // Expand master key to 1st FN 96-bit key
    expand_1st_key(key1, master_key);
    
    // Add extra bits for s-boxes with less than 6 inputs
    key1[0] ^= BIT(key1[0], 1) <<  4;
    key1[0] ^= BIT(key1[0], 2) <<  5;
    key1[0] ^= BIT(key1[0], 8) << 11;
    key1[1] ^= BIT(key1[1], 0) <<  5;
    key1[1] ^= BIT(key1[1], 8) << 11;
    key1[2] ^= BIT(key1[2], 1) <<  5;
    key1[2] ^= BIT(key1[2], 8) << 11;
    
    // Decrypt for each 16-bit address (low 16 bits of address)
    for (u32 i = 0; i < 0x10000; ++i) {
        u16 seed;
        u32 subkey[2];
        u32 key2[4];
        
        // Pass the address through FN1
        seed = feistel(static_cast<u16>(i), fn1_groupA, fn1_groupB,
                      &sboxes1[0 * 4], &sboxes1[1 * 4], &sboxes1[2 * 4], &sboxes1[3 * 4],
                      key1[0], key1[1], key1[2], key1[3]);
        
        // Expand the result to 64-bit
        expand_subkey(subkey, seed);
        
        // XOR with the master key
        subkey[0] ^= master_key[0];
        subkey[1] ^= master_key[1];
        
        // Expand key to 2nd FN 96-bit key
        expand_2nd_key(key2, subkey);
        
        // Add extra bits for s-boxes with less than 6 inputs
        key2[0] ^= BIT(key2[0], 0) <<  5;
        key2[0] ^= BIT(key2[0], 6) << 11;
        key2[1] ^= BIT(key2[1], 0) <<  5;
        key2[1] ^= BIT(key2[1], 1) <<  4;
        key2[2] ^= BIT(key2[2], 2) <<  5;
        key2[2] ^= BIT(key2[2], 3) <<  4;
        key2[2] ^= BIT(key2[2], 7) << 11;
        key2[3] ^= BIT(key2[3], 1) <<  5;
        
        // Decrypt the opcodes at addresses matching this low 16 bits
        for (u32 a = i; a < wordLength; a += 0x10000) {
            if (a >= lower_limit && a <= upper_limit) {
                // Read encrypted word (little-endian as stored in ZIP)
                u16 encrypted = (static_cast<u16>(rom[a * 2 + 1]) << 8) | rom[a * 2];
                
                // Decrypt using second Feistel network
                u16 decrypted = feistel(encrypted, fn2_groupA, fn2_groupB,
                                       &sboxes2[0 * 4], &sboxes2[1 * 4],
                                       &sboxes2[2 * 4], &sboxes2[3 * 4],
                                       key2[0], key2[1], key2[2], key2[3]);
                
                // Write decrypted word (little-endian, will be byteswapped later)
                dec[a * 2] = static_cast<u8>(decrypted & 0xFF);
                dec[a * 2 + 1] = static_cast<u8>(decrypted >> 8);
            } else {
                // Copy unchanged (outside decryption range)
                dec[a * 2] = rom[a * 2];
                dec[a * 2 + 1] = rom[a * 2 + 1];
            }
        }
    }
}

// ========================================================
// CPS1 decryption functions
// ========================================================

static u32 bitswap1(u32 src, u32 key, u32 select) {
    if (select & (1 << ((key >> 0) & 7)))
        src = (src & 0xfc) | ((src & 0x01) << 1) | ((src & 0x02) >> 1);
    if (select & (1 << ((key >> 4) & 7)))
        src = (src & 0xf3) | ((src & 0x04) << 1) | ((src & 0x08) >> 1);
    if (select & (1 << ((key >> 8) & 7)))
        src = (src & 0xcf) | ((src & 0x10) << 1) | ((src & 0x20) >> 1);
    if (select & (1 << ((key >> 12) & 7)))
        src = (src & 0x3f) | ((src & 0x40) << 1) | ((src & 0x80) >> 1);

    return src;
}

static u32 bitswap2(u32 src, u32 key, u32 select) {
    if (select & (1 << ((key >> 12) & 7)))
        src = (src & 0xfc) | ((src & 0x01) << 1) | ((src & 0x02) >> 1);
    if (select & (1 << ((key >> 8) & 7)))
        src = (src & 0xf3) | ((src & 0x04) << 1) | ((src & 0x08) >> 1);
    if (select & (1 << ((key >> 4) & 7)))
        src = (src & 0xcf) | ((src & 0x10) << 1) | ((src & 0x20) >> 1);
    if (select & (1 << ((key >> 0) & 7)))
        src = (src & 0x3f) | ((src & 0x40) << 1) | ((src & 0x80) >> 1);

    return src;
}

static u32 bytedecode(u32 src, u32 swap_key1, u32 swap_key2, u32 xor_key, u32 select)
{
    src = bitswap1(src, swap_key1 & 0xffff, select & 0xff);
    src = ((src & 0x7f) << 1) | ((src & 0x80) >> 7);
    src = bitswap2(src, swap_key1 >> 16, select & 0xff);
    src ^= xor_key;
    src = ((src & 0x7f) << 1) | ((src & 0x80) >> 7);
    src = bitswap2(src, swap_key2 & 0xffff, select >> 8);
    src = ((src & 0x7f) << 1) | ((src & 0x80) >> 7);
    src = bitswap1(src, swap_key2 >> 16, select >> 8);
    return src;
}

static void kabuki_decode(u8 *src, u8 *dest_op, u8 *dest_data, u32 base_addr, u32 length, u32 swap_key1, u32 swap_key2, u32 addr_key, u32 xor_key) {
    u32 A;
    u32 select;

    for (A = 0; A < length; A++) {
        /* decode opcodes */
        select = (A + base_addr) + addr_key;
        dest_op[A] = static_cast<u8>(bytedecode(src[A], swap_key1, swap_key2, xor_key, select));

        /* decode data */
        select = ((A + base_addr) ^ 0x1fc0) + addr_key + 1;
        dest_data[A] = static_cast<u8>(bytedecode(src[A], swap_key1, swap_key2, xor_key, select));
    }
}

void decryptCPS1(u32 swap_key1, u32 swap_key2, u32 addr_key, u32 xor_key, u8* rom, u32 length) {
    // For QSound games, the ROM buffer is doubled in size
    // First half: opcode arg
    // Second half: opcode
    u32 diff = length / 2;

    kabuki_decode(rom, rom + diff, rom, 0x0000, 0x8000, swap_key1, swap_key2, addr_key, xor_key);
}

} // namespace cps
