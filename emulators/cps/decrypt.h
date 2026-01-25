#pragma once

#include "../../types.h"

namespace cps {

// CPS2 decryption functions
// All credit goes to Andreas Naive for breaking the encryption algorithm

// Decrypt CPS2 program ROM
// master_key: 64-bit encryption key (2x32-bit values)
// lower_limit: Lower address limit (in 16-bit words)
// upper_limit: Upper address limit (in 16-bit words)
// rom: Pointer to encrypted ROM data (bytes, big-endian 16-bit words)
// dec: Pointer to output buffer for decrypted ROM (bytes, big-endian 16-bit words)
// length: Length of ROM in bytes
void decryptCPS2(const u32* master_key, u32 lower_limit, u32 upper_limit,
                 const u8* rom, u8* dec, u32 length);

// CPS1 decryption functions
void decryptCPS1(u32 swap_key1, u32 swap_key2, u32 addr_key, u32 xor_key, u8* rom, u32 length);
} // namespace cps
