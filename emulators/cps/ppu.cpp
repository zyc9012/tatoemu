#include "ppu.h"
#include "cartridge.h"
#include "cpu.h"
#include "memory.h"
#include "consts.h"
#include <cstring>
#include <iostream>
#include <algorithm>

/*
 * PPU Implementation (CPS1 and CPS2)
 * ===================================
 * 
 * RENDERING ARCHITECTURE
 * ----------------------
 * The PPU handles rendering of:
 * - 3 scroll layers (background tiles)
 * - 1 sprite layer (objects)
 * - Star field (CPS1 only, some games)
 * 
 * Tile Sizes:
 * - Scroll 1: 8x8 tiles (text layer)
 * - Scroll 2: 16x16 tiles (main background, supports row scroll)
 * - Scroll 3: 32x32 tiles (large background elements)
 * - Sprites: 16x16 tiles (variable size via linking)
 * 
 * Graphics Format (Same for CPS1 and CPS2):
 * - 4bpp (16 colors per tile)
 * - Stored in planar format in ROM (4 ROM chips per bank)
 * - Needs decoding for efficient rendering
 * 
 * 
 * KEY DIFFERENCES BETWEEN CPS1 AND CPS2
 * ======================================
 * 
 * 1. LAYER PRIORITY SYSTEM
 * -------------------------
 * CPS1 (Cps1Layers):
 * - Simple priority system
 * - Layer order determined by layer control register bits [13:6]
 * - Sprites drawn as a single layer between scroll layers
 * - Optional "BgHi" masking for scroll layers over sprites
 * - Star field support (2 layers)
 * 
 * CPS2 (Cps2Layers):
 * - Complex raster-based priority system
 * - Supports multiple raster interrupt zones (MAX_RASTER)
 * - Each zone can have different layer priorities
 * - 8-level sprite priority system (priorities 0-7)
 * - Sprites interleaved with scroll layers based on priority
 * - Uses Z-buffer to track sprite priority
 * - Layer-sprite priority register at 0x400004 (CpsSaveFrg[4-5])
 * - No star field support
 * 
 * 
 * 2. SCROLL LAYER RENDERING
 * --------------------------
 * CPS1 (Cps1Scr1Draw, Cps1Scr3Draw):
 * - Renders entire screen at once
 * - Simple clipping for border tiles
 * - Optional tile masking (CpstPmsk) for BgHi mode
 * - Uses graphics bank mapper for tile addressing
 * 
 * CPS2 (Cps2Scr1Draw, Cps2Scr3Draw):
 * - Supports partial scanline rendering (nStartline to nEndline)
 * - Optimized for raster effects
 * - No BgHi masking
 * - Direct graphics ROM addressing (no mapper in practice, though code path exists)
 * - Scroll 3 has game-specific tile offset hacks:
 *   - Xmcota: tile >= 0x5800 -> subtract 0x4000
 *   - Ssf2t: tile < 0x5600 -> add 0x4000
 * - Uses CpstOneDoX[2] instead of CpstOneDoX[nBgHi]
 * 
 * 
 * 3. SPRITE RENDERING
 * -------------------
 * CPS1 (Cps1ObjDraw):
 * - 256 sprites maximum
 * - Simple reverse-order rendering (unless CpsDrawSpritesInReverse set)
 * - Sprite list at address from register 0x00 (CpsReg)
 * - Single-pass rendering
 * - Sprite offsets: -0x40 X, -0x10 Y (plus global offsets)
 * - Optional sprite blending (if .bld file present)
 * - Uses CpstOneObjDoX[0] for all sprites
 * 
 * CPS2 (Cps2ObjDraw):
 * - 1024 sprites maximum
 * - Z-buffer based priority system with 8 levels (0-7)
 * - Sprite priority from bits [15:13] of word 0
 * - Sprites rendered in multiple passes by priority level
 * - Sprite list double-buffered at CpsRam708 + ((nCpsObjectBank ^ 1) << 15)
 * - Sprite offsets from CpsSaveFrg[0][0x9] and CpsSaveFrg[0][0xB]
 * - Z-buffer tracks which sprites have been drawn (ZBuf, ZValue)
 * - Masking for sprites that need to draw behind other sprites
 * - Uses CpstOneObjDoX[0] (normal) or CpstOneObjDoX[1] (with masking)
 * - Optional sprite blending (if .bld file present)
 * - Marvel vs Capcom offset hack: if attrib & 0x80, add CpsSaveFrg[0][0x9]
 * 
 * 
 * 4. PALETTE HANDLING
 * -------------------
 * Both use same 16-bit format: [brightness:4][blue:4][green:4][red:4]
 * 
 * CPS1 (CpsPalUpdate):
 * - Always updates all 6 palette pages
 * - Palette control register may not be reliable
 * - Palette index XOR 15 for color lookup
 * 
 * CPS2 (CpsPalUpdate):
 * - Palette control register at nCpsPalCtrlReg (usually 0x0A)
 * - Only updates pages with their bit set in control register
 * - Bit 0 = page 0, bit 1 = page 1, etc.
 * - Palette index XOR 15 for color lookup
 * 
 * 
 * 5. SCREEN CLEARING
 * ------------------
 * CPS1 (CpsClearScreen):
 * - Clears to palette entry 0xBFF ^ 15
 * - Can optionally clear to black if fFakeDip & 1
 * 
 * CPS2 (CpsClearScreen):
 * - Always clears to black (memset to 0)
 * 
 * 
 * 6. RASTER EFFECTS
 * -----------------
 * CPS1:
 * - No raster interrupt support
 * - Single set of scroll registers for entire frame
 * 
 * CPS2:
 * - Supports raster interrupts (MAX_RASTER zones)
 * - Each zone has its own register set (CpsSaveReg[nSlice])
 * - Can change scroll positions, layer priorities mid-frame
 * - nRasterline[] array defines scanline boundaries
 * - nStartline/nEndline passed to drawing functions
 * 
 * 
 * IMPLEMENTATION NOTES
 * ====================
 * - Our implementation is currently CPS1-focused
 * - CPS2-specific features need to be added:
 *   - Z-buffer for sprite priority
 *   - Raster interrupt support
 *   - Partial scanline rendering
 *   - Different sprite data source
 *   - Different clear screen behavior
 * 
 * 
 * References:
 * -----------
 * - FBNeo cps_draw.cpp (DrawFnInit, Cps1Layers, Cps2Layers, CpsClearScreen)
 * - FBNeo cps_scr.cpp (Cps1Scr1Draw, Cps2Scr1Draw, Cps1Scr3Draw, Cps2Scr3Draw)
 * - FBNeo cps_obj.cpp (Cps1ObjDraw, Cps2ObjDraw, CpsObjDrawInit)
 * - FBNeo cps_pal.cpp (CpsPalUpdate palette control register)
 */

namespace cps {

// ============================================================================
// Separation table for graphics decoding
// Converts a byte to spread-out bits: ABCDEFGH -> A00B00C00D00E00F00G00H00
// ============================================================================

// Precalculated separation table
static u32 SepTable[256];
static bool SepTableInitialized = false;

static inline u32 Separate(u32 b) {
    u32 a = b;                                      // 00000000 00000000 00000000 ABCDEFGH
    a = ((a & 0x000000F0) << 12) | (a & 0x0000000F); // 00000000 0000ABCD 00000000 0000EFGH
    a = ((a & 0x000C000C) <<  6) | (a & 0x00030003); // 00000AB0 00000CD0 00000EF0 00000GH0
    a = ((a & 0x02020202) <<  3) | (a & 0x01010101); // 000A000B 000C000D 000E000F 000G000H
    return a;
}

static void InitSepTable() {
    if (SepTableInitialized) return;
    for (int i = 0; i < 256; i++) {
        SepTable[i] = Separate(255 - i);  // Inverted for CPS1 palette indexing
    }
    SepTableInitialized = true;
}

// ============================================================================
// Constructor / Initialization
// ============================================================================

PPU::PPU()
    : m_cpu(nullptr)
    , m_cartridge(nullptr)
    , m_memory(nullptr)
    , m_videoDevice(nullptr)
    , m_gfxLen(0)
    , m_gfxMask(0)
    , m_layer1XOffs(0), m_layer1YOffs(0)
    , m_layer2XOffs(0), m_layer2YOffs(0)
    , m_layer3XOffs(0), m_layer3YOffs(0)
    , m_globalXOffs(0), m_globalYOffs(0)
    , m_gfxMapper(nullptr)
    , m_frameComplete(false)
    , m_scanline(0)
    , m_cycles(0)
    , m_paletteNeedsUpdate(true)
    , m_maxZValue(1)
    , m_maxZMask(0)
    , m_zOffset(0)
    , m_currentZValue(0)
    , m_currentMask(0)
    , m_bgHiMode(false)
    , m_spriteEnableMask(0xFF)
    , m_objectBank(0)
{
    InitSepTable();
    
    m_frameBuffer.fill(0);
    m_vram.fill(0);
    m_palette.fill(0);
    m_cpsRegs.fill(0);
    m_starField.fill(0);
    m_rasterLines.fill(0);
    m_maskAddr.fill(0);
    
    for (auto& regs : m_rasterRegs) {
        regs.fill(0);
    }
    for (auto& frg : m_rasterFrg) {
        frg.fill(0);
    }
    
    m_gfxScroll[0] = 0;
    m_gfxScroll[1] = 0;
    m_gfxScroll[2] = 0;
    m_gfxScroll[3] = 0;
    
    m_gfxBankSizes[0] = 0;
    m_gfxBankSizes[1] = 0;
    m_gfxBankSizes[2] = 0;
    m_gfxBankSizes[3] = 0;
}

void PPU::setCartridge(Cartridge* cartridge) {
    m_cartridge = cartridge;
}

void PPU::reset() {
    m_frameBuffer.fill(0);
    m_vram.fill(0);
    m_cpsRegs.fill(0);
    m_starField.fill(0);
    m_rasterLines.fill(0);
    m_maskAddr.fill(0);
    
    m_frameComplete = false;
    m_scanline = 0;
    m_cycles = 0;
    m_paletteNeedsUpdate = true;
    
    // Reset scroll offsets
    m_layer1XOffs = 0; m_layer1YOffs = 0;
    m_layer2XOffs = 0; m_layer2YOffs = 0;
    m_layer3XOffs = 0; m_layer3YOffs = 0;
    m_globalXOffs = 0; m_globalYOffs = 0;
    
    // Reset CPS2 Z-buffer state
    m_maxZValue = 1;
    m_maxZMask = 0;
    m_zOffset = 0;
    m_currentZValue = 0;
    
    // Reset masking and blending
    m_currentMask = 0;
    m_bgHiMode = false;
    m_spriteEnableMask = 0xFF;
    m_objectBank = 0;
    
    // Reset raster zones (default: single zone covering whole screen)
    m_rasterLines[0] = 0;        // Zone 0 starts at scanline 0
    m_rasterLines[1] = 0;        // No additional zones by default
    for (s32 i = 2; i < MAX_RASTER + 2; i++) {
        m_rasterLines[i] = 0;
    }
    
    if (m_cartridge) {
        u8 cpsVer = m_cartridge->getCPSVersion();
        
        if (cpsVer == 1) {
            // CPS1-specific: Board configuration and graphics mapper
            m_boardConfig = m_cartridge->getBoardConfig();
            setupGfxMapper();
        } else {
            // CPS2-specific: Initialize Z-buffer
            m_zBuffer.resize(SCREEN_WIDTH * SCREEN_HEIGHT, 0);
        }
        
        decodeGraphicsROM();
    }
}

void PPU::setupGfxMapper() {
    if (!m_cartridge) return;
    
    CPSMapper mapper = m_cartridge->getMapper();
    
    // Get mapper table and bank sizes from database
    m_gfxMapper = GameDatabase::getGfxMapperTable(mapper);
    if (!m_gfxMapper) {
        throw std::runtime_error("Unsupported mapper");
    }
    
    GameDatabase::getGfxBankSizes(mapper, m_gfxBankSizes);
}

// ============================================================================
// Graphics ROM Decoding
// Converts CPS1/CPS2 4bpp planar format to linear nibbles for fast rendering
// Both CPS1 and CPS2 use the same graphics ROM format
// ============================================================================

void PPU::decodeGraphicsROM() {
    if (!m_cartridge) return;
    
    u32 srcSize = m_cartridge->getGraphicsROMSize();
    if (srcSize == 0) {
        std::cerr << "PPU: No graphics ROM data to decode" << std::endl;
        return;
    }
    
    /*
     * CPS1/CPS2 Graphics ROM Decoding
     * ==========================================================
     * 
     * Both CPS1 and CPS2 use the same graphics ROM format:
     * Graphics ROMs are organized as groups of 4 chips (512KB each):
     * - ROM 0: Left half of tiles, bits 0-1
     * - ROM 1: Left half of tiles, bits 2-3
     * - ROM 2: Right half of tiles, bits 0-1
     * - ROM 3: Right half of tiles, bits 2-3
     * 
     * Each ROM provides 2 bits per pixel. Reading 2 bytes from a ROM
     * gives 2 bits for 8 pixels.
     * 
     * Output format has 8-byte stride:
     * - Bytes 0-3: Left half pixels (32-bit word, 8 nibbles)
     * - Bytes 4-7: Right half pixels (32-bit word, 8 nibbles)
     * 
     * For each group of 4 ROM chips (2MB total):
     * - Process 0x80000 pairs of bytes from each ROM
     * - Combine ROM0+ROM1 for left pixels, ROM2+ROM3 for right pixels
     */
    
    // Output size: same as input size (1:1 ratio)
    // 4 ROM chips of 512KB each = 2MB input → 2MB output per bank
    // The 8-byte stride stores left (offset 0) and right (offset 4) halves
    u32 dstSize = srcSize;
    
    m_decodedGfx.resize(dstSize, 0);
    m_gfxLen = dstSize;
    
    // Calculate mask for graphics address
    m_gfxMask = 1;
    while (m_gfxMask < m_gfxLen) {
        m_gfxMask <<= 1;
    }
    m_gfxMask -= 1;
    
    // Process graphics in 2MB banks (4 ROMs of 512KB each)
    // - Left half: ROM 0 (bits 0-1, shift 0) OR ROM 1 (bits 0-1, shift 2)
    // - Right half: ROM 2 (bits 0-1, shift 0) OR ROM 3 (bits 0-1, shift 2)
    u32 romChipSize = 0x80000;  // 512KB per ROM chip
    u32 numBanks = srcSize / (romChipSize * 4);
    if (numBanks == 0) numBanks = 1;  // At least one partial bank
    
    for (u32 bank = 0; bank < numBanks; bank++) {
        u32 bankBase = bank * romChipSize * 4;
        u32 outBase = bank * romChipSize * 4;  // Output is same size as input
        
        // ROM offsets within this bank
        u32 rom0 = bankBase + 0 * romChipSize;  // Left half, bits 0-1
        u32 rom1 = bankBase + 1 * romChipSize;  // Left half, bits 2-3
        u32 rom2 = bankBase + 2 * romChipSize;  // Right half, bits 0-1
        u32 rom3 = bankBase + 3 * romChipSize;  // Right half, bits 2-3
        
        // Process each byte pair (matching CpsLoadOne with nWord=1)
        // Each iteration processes 2 bytes from ROM → 8 pixels worth of 2 bits
        for (u32 i = 0; i < romChipSize && (rom0 + i + 1) < srcSize; i += 2) {
            u32 outOffset = outBase + (i / 2) * 8;  // 8-byte stride
            
            // Left half (offset 0): Process ROM 0 then OR ROM 1
            if (rom0 + i + 1 < srcSize) {
                u8 b0 = m_cartridge->readGraphicsROM8(rom0 + i);
                u8 b1 = m_cartridge->readGraphicsROM8(rom0 + i + 1);
                // CpsLoadOne: first byte → bit 0, second byte → bit 1
                u32 pix = SepTable[b0] | (SepTable[b1] << 1);
                // Shift 0 (already done)
                if (outOffset < m_decodedGfx.size()) {
                    *reinterpret_cast<u32*>(m_decodedGfx.data() + outOffset) |= pix;
                }
            }
            
            if (rom1 + i + 1 < srcSize) {
                u8 b0 = m_cartridge->readGraphicsROM8(rom1 + i);
                u8 b1 = m_cartridge->readGraphicsROM8(rom1 + i + 1);
                u32 pix = SepTable[b0] | (SepTable[b1] << 1);
                pix <<= 2;  // Shift 2 for bits 2-3
                if (outOffset < m_decodedGfx.size()) {
                    *reinterpret_cast<u32*>(m_decodedGfx.data() + outOffset) |= pix;
                }
            }
            
            // Right half (offset 4): Process ROM 2 then OR ROM 3
            if (rom2 + i + 1 < srcSize) {
                u8 b0 = m_cartridge->readGraphicsROM8(rom2 + i);
                u8 b1 = m_cartridge->readGraphicsROM8(rom2 + i + 1);
                u32 pix = SepTable[b0] | (SepTable[b1] << 1);
                // Shift 0
                if (outOffset + 4 < m_decodedGfx.size()) {
                    *reinterpret_cast<u32*>(m_decodedGfx.data() + outOffset + 4) |= pix;
                }
            }
            
            if (rom3 + i + 1 < srcSize) {
                u8 b0 = m_cartridge->readGraphicsROM8(rom3 + i);
                u8 b1 = m_cartridge->readGraphicsROM8(rom3 + i + 1);
                u32 pix = SepTable[b0] | (SepTable[b1] << 1);
                pix <<= 2;  // Shift 2 for bits 2-3
                if (outOffset + 4 < m_decodedGfx.size()) {
                    *reinterpret_cast<u32*>(m_decodedGfx.data() + outOffset + 4) |= pix;
                }
            }
        }
    }
}

// ============================================================================
// VRAM Access
// ============================================================================

u8 PPU::readVRAM8(u32 address) {
    if (address < VRAM_SIZE) {
        return m_vram[address];
    }
    return 0;
}

u16 PPU::readVRAM16(u32 address) {
    if (address + 1 < VRAM_SIZE) {
        // CPS1 VRAM is big-endian from 68000 perspective
        return (static_cast<u16>(m_vram[address]) << 8) | m_vram[address + 1];
    }
    return 0;
}

u32 PPU::readVRAM32(u32 address) {
    u32 high = readVRAM16(address);
    u32 low = readVRAM16(address + 2);
    return (high << 16) | low;
}

void PPU::writeVRAM8(u32 address, u8 value) {
    if (address < VRAM_SIZE) {
        m_vram[address] = value;
        // Always mark palette as potentially dirty - the actual palette location
        // depends on CPS registers which can change
        m_paletteNeedsUpdate = true;
    }
}

void PPU::writeVRAM16(u32 address, u16 value) {
    if (address + 1 < VRAM_SIZE) {
        // Big-endian write
        m_vram[address] = (value >> 8) & 0xFF;
        m_vram[address + 1] = value & 0xFF;
        m_paletteNeedsUpdate = true;
    }
}

void PPU::writeVRAM32(u32 address, u32 value) {
    writeVRAM16(address, (value >> 16) & 0xFFFF);
    writeVRAM16(address + 2, value & 0xFFFF);
}

// ============================================================================
// CPS Register Access
// ============================================================================

u8 PPU::readRegister8(u8 reg) {
    return m_cpsRegs[reg];
}

void PPU::writeRegister8(u8 reg, u8 value) {
    m_cpsRegs[reg] = value;
}

// ============================================================================
// VRAM Helper - Find graphics RAM at a specific address
// ============================================================================

u8* PPU::findGfxRam(u32 address, u32 len) {
    address &= 0xFFFFFF;  // 24-bit bus
    // VRAM is at 0x900000-0x92FFFF from 68000 address space
    // In our VRAM array, offset 0 corresponds to 0x900000
    if (address >= 0x900000 && address + len <= 0x930000) {
        return m_vram.data() + (address - 0x900000);
    }
    return nullptr;
}

// ============================================================================
// Graphics ROM Bank Mapper
// Maps tile codes to actual ROM addresses based on graphics type
// ============================================================================

s32 PPU::gfxRomBankMapper(u32 type, s32 code) const {
    // CPS2: Direct addressing, no mapper needed
    if (!m_cartridge || m_cartridge->getCPSVersion() != 1) {
        return code;
    }
    
    // CPS1: Use graphics bank mapper
    if (!m_gfxMapper) return code;
    
    s32 shift = 0;
    switch (type) {
        case GFXTYPE_SPRITES: shift = 1; break;
        case GFXTYPE_SCROLL1: shift = 0; break;
        case GFXTYPE_SCROLL2: shift = 1; break;
        case GFXTYPE_SCROLL3: shift = 3; break;
        default: shift = 0; break;
    }
    
    s32 shiftedCode = code << shift;
    
    const GfxRange* range = m_gfxMapper;
    while (range->type) {
        // Match against shifted code range
        if ((range->type & type) &&
            shiftedCode >= static_cast<s32>(range->start) && 
            shiftedCode <= static_cast<s32>(range->end)) {
            // Found matching range
            // Calculate base address by summing bank sizes before target bank
            s32 base = 0;
            for (u32 i = 0; i < range->bank; i++) {
                base += m_gfxBankSizes[i];
            }
            
            // Add code (masked by bank size) to base, then shift right
            s32 bankSize = m_gfxBankSizes[range->bank];
            if (bankSize > 0) {
                return (base + (shiftedCode & (bankSize - 1))) >> shift;
            }
        }
        range++;
    }
    
    // Not found - return -1 to indicate invalid tile
    return -1;
}

const u8* PPU::getGfxRom(u32 address) const {
    if (address < m_gfxLen) {
        return m_decodedGfx.data() + address;
    }
    return nullptr;
}

// ============================================================================
// CPS2 Z-Buffer Initialization
// ============================================================================

void PPU::initCPS2ZBuffer() {
    // Initialize Z-buffer offset for this frame
    m_zOffset = m_maxZMask;
    
    // Check if Z-buffer might overflow
    if (m_zOffset >= 0xFC00) {
        // Clear Z-buffer to prevent overflow
        std::fill(m_zBuffer.begin(), m_zBuffer.end(), 0);
        m_zOffset = 0;
    }
    
    m_maxZValue = m_zOffset + 1;
    m_maxZMask = m_zOffset;
}

// ============================================================================
// Palette Handling
// ============================================================================

u32 PPU::convertPaletteEntry(u16 entry) {
    /*
     * CPS1/CPS2 palette format (16-bit):
     * Bits 15-12: Brightness (0-15)
     * Bits 11-8:  Blue (0-15)
     * Bits 7-4:   Green (0-15)
     * Bits 3-0:   Red (0-15)
     */
    
    s32 bright = 0x0F + ((entry >> 12) << 1);
    
    s32 r = ((entry >> 8) & 0x0F) * 0x11 * bright / 0x2D;
    s32 g = ((entry >> 4) & 0x0F) * 0x11 * bright / 0x2D;
    s32 b = ((entry >> 0) & 0x0F) * 0x11 * bright / 0x2D;
    
    // Clamp to 0-255
    r = std::min(255, std::max(0, r));
    g = std::min(255, std::max(0, g));
    b = std::min(255, std::max(0, b));
    
    // Return as ARGB8888
    return 0xFF000000 | (r << 16) | (g << 8) | b;
}

void PPU::updatePalette() {
    if (!m_cartridge) return;
    
    u8 cpsVer = m_cartridge->getCPSVersion();
    
    // Get palette base address from CPS registers (register 0x0A as 16-bit)
    // The 68000 writes this as big-endian, so reg[0x0A] is high byte, reg[0x0B] is low byte
    u32 palAddr = (static_cast<u32>(m_cpsRegs[0x0A]) << 8) | m_cpsRegs[0x0B];
    palAddr <<= 8;           // Multiply by 256 to get actual address
    palAddr &= 0xFFFC00;     // Align to 1KB boundary
    
    // Palette is stored in VRAM - convert from absolute address to VRAM offset
    // VRAM starts at 0x900000 in 68000 address space
    u32 palOffset = 0;
    if (palAddr >= 0x900000) {
        palOffset = palAddr - 0x900000;
    }

    if (palOffset >= VRAM_SIZE) {
        palOffset = 0;  // Fallback
        std::cout << "Palette Update: palOffset out of bounds: 0x" << std::hex << palOffset << std::dec << std::endl;
    }
    
    // Get palette control register (which pages to update)
    // For now, always update all pages for both CPS1 and CPS2
    // TODO: CPS2 palette control register location is game-specific (nCpsPalCtrlReg)
    // and needs to be configured per game. For now, always update all pages.
    u8 palCtrl = 0xFF;  // Update all pages
    
    // Convert each 16-bit palette entry
    // Both CPS1 and CPS2 have 6 pages of 512 colors = 3072 entries (0xC00)
    // Each page is 0x400 bytes (512 entries * 2 bytes each)
    for (u32 page = 0; page < 6; page++) {
        // Always update all pages (palette control reg location is game-specific)
        if (palCtrl & (1 << page)) {
            for (u32 i = 0; i < 0x200; i++) {
                u32 srcOffset = palOffset + (page * 0x400) + (i * 2);
                if (srcOffset + 1 < VRAM_SIZE) {
                    // Read 16-bit palette entry (big-endian)
                    u16 entry = (static_cast<u16>(m_vram[srcOffset]) << 8) | 
                                m_vram[srcOffset + 1];
                    
                    // Both CPS1 and CPS2 palette use XOR 15 for color indexing
                    u32 dstIndex = (page * 0x200) + (i ^ 15);
                    if (dstIndex < m_palette.size()) {
                        m_palette[dstIndex] = convertPaletteEntry(entry);
                    }
                }
            }
        }
    }
    
    m_paletteNeedsUpdate = false;
}

// ============================================================================
// Frame Stepping
// ============================================================================

void PPU::step() {
    // CPS1/CPS2 render a full frame at VBlank
    // Frame rate is ~59.63Hz for both systems
    // CPS1: 68000 runs at 10MHz, CPS2: 68000 runs at 16MHz
    // CPU cycles per frame ~= CPU_FREQUENCY / 59.63
    
    // Increment cycle counter
    m_cycles++;
    
    // Both systems typically trigger VBlank interrupt at scanline 224
    // Each scanline takes approximately CPU_CYCLES_PER_FRAME / 262 cycles
    // Total active lines: 224, VBlank: 38 lines (262 total)
    
    constexpr u32 CYCLES_PER_SCANLINE = 640;  // Approximate, works for both
    constexpr u32 VISIBLE_SCANLINES = 224;
    constexpr u32 TOTAL_SCANLINES = 262;
    constexpr u32 CYCLES_PER_FRAME = CYCLES_PER_SCANLINE * TOTAL_SCANLINES;
    
    // Calculate current scanline
    u32 newScanline = m_cycles / CYCLES_PER_SCANLINE;
    
    // Check if we've moved to a new scanline
    if (newScanline != m_scanline) {
        u32 prevScanline = m_scanline;
        m_scanline = newScanline;
        
        // CPS2: Check for raster interrupts
        if (m_cartridge && m_cartridge->getCPSVersion() == 2 && m_memory && m_cpu) {
            // Check IRQ line 50 (priority 4)
            u16 rasterIRQ50 = m_memory->getRasterIRQ50();
            if (rasterIRQ50 > 0 && rasterIRQ50 < VISIBLE_SCANLINES) {
                // Check if we just crossed this scanline
                if (prevScanline < rasterIRQ50 && m_scanline >= rasterIRQ50) {
                    // Save current register state to zone 0 before IRQ
                    copyRegistersToZone(0);
                    copyFrgRegistersToZone(0);
                    
                    // Set raster line boundary (zone 1 starts here)
                    setRasterLine(1, static_cast<s32>(rasterIRQ50));
                    
                    // Trigger IRQ line 50 (priority 4)
                    m_cpu->irq(4);
                }
            }
            
            // Check IRQ line 52 (priority 6)
            u16 rasterIRQ52 = m_memory->getRasterIRQ52();
            if (rasterIRQ52 > 0 && rasterIRQ52 < VISIBLE_SCANLINES) {
                // Check if we just crossed this scanline
                if (prevScanline < rasterIRQ52 && m_scanline >= rasterIRQ52) {
                    // Determine which zone we're entering
                    s32 zoneNum = (rasterIRQ50 > 0 && rasterIRQ52 > rasterIRQ50) ? 2 : 1;
                    
                    // Save previous zone's register state
                    copyRegistersToZone(zoneNum - 1);
                    copyFrgRegistersToZone(zoneNum - 1);
                    
                    // Set raster line boundary
                    setRasterLine(zoneNum, static_cast<s32>(rasterIRQ52));
                    
                    // Trigger IRQ line 52 (priority 6)
                    m_cpu->irq(6);
                }
            }
        }
        
        // At VBlank start (scanline 224), render the frame and trigger interrupt
        if (m_scanline == VISIBLE_SCANLINES) {
            // CPS2: Save final zone's register state before rendering
            if (m_cartridge && m_cartridge->getCPSVersion() == 2) {
                s32 lastZone = getRasterLineCount() - 1;
                if (lastZone >= 0) {
                    copyRegistersToZone(lastZone);
                    copyFrgRegistersToZone(lastZone);
                }
            }
            
            renderFrame();

            // Trigger VBlank interrupt (priority 2)
            if (m_cpu) {
                m_cpu->irq(2);
            }
        }
    }
    
    // Check if frame is complete
    if (m_cycles >= CYCLES_PER_FRAME) {
        m_cycles = 0;
        m_scanline = 0;
        m_frameComplete = true;
    }
}

// ============================================================================
// Frame Rendering
// ============================================================================

void PPU::renderFrame() {
    if (!m_videoDevice) return;
    
    // Update palette if needed
    updatePalette();
    
    // Clear screen with background color
    clearScreen();
    
    // Render all layers
    renderLayers();
    
    // Copy frame buffer to video device
    m_videoDevice->render(m_frameBuffer.data());
}

void PPU::clearScreen() {
    if (!m_cartridge) {
        m_frameBuffer.fill(0);
        return;
    }
    
    u8 cpsVer = m_cartridge->getCPSVersion();
    
    if (cpsVer == 1) {
        // CPS1: Clear to palette entry 0xBFF ^ 15 (background color)
        // Can optionally clear to black with fFakeDip & 1, but we don't support that
        u32 bgColor = m_palette[0xBFF ^ 15];
        m_frameBuffer.fill(bgColor);
    } else {
        // CPS2: Always clear to black
        m_frameBuffer.fill(0);
    }
}

// ============================================================================
// Layer Rendering
// ============================================================================

void PPU::renderLayers() {
    if (!m_cartridge) return;
    
    u8 cpsVer = m_cartridge->getCPSVersion();
    
    if (cpsVer == 1) {
        renderLayersCPS1();
    } else {
        renderLayersCPS2();
    }
}

void PPU::renderLayersCPS1() {
    // CPS1: Simple priority system with single register set
    
    // Use board configuration for layer control register
    u8 lcReg = m_boardConfig.layerControlReg;
    u16 layerCtrl = (static_cast<u16>(m_cpsRegs[lcReg]) << 8) | m_cpsRegs[lcReg + 1];
    
    // Determine which layers are enabled using board-specific enable bits
    bool layer1Enable = (layerCtrl & m_boardConfig.layerEnable[0]) != 0;
    bool layer2Enable = (layerCtrl & m_boardConfig.layerEnable[1]) != 0;
    bool layer3Enable = (layerCtrl & m_boardConfig.layerEnable[2]) != 0;
    
    // Extract layer priority order (from layer control register)
    // Bits 13-12: Top layer
    // Bits 11-10: Second layer
    // Bits 9-8: Third layer
    // Bits 7-6: Bottom layer
    s32 draw[4];
    draw[0] = (layerCtrl >> 12) & 3;  // Top layer
    draw[1] = (layerCtrl >> 10) & 3;
    draw[2] = (layerCtrl >> 8) & 3;
    draw[3] = (layerCtrl >> 6) & 3;   // Bottom layer
    
    // Build enable mask
    u32 drawMask = 1;  // Sprites always on
    if (layer1Enable) drawMask |= 2;
    if (layer2Enable) drawMask |= 4;
    if (layer3Enable) drawMask |= 8;
    
    // Check for repeated layers and remove duplicates
    for (int i = 0; i < 3; i++) {
        for (int j = i + 1; j < 4; j++) {
            if (draw[i] == draw[j]) draw[j] = -1;
        }
    }
    
    // Get scroll layer base addresses from registers
    u32 scr1Off = ((static_cast<u32>(m_cpsRegs[0x02]) << 8) | m_cpsRegs[0x03]) << 8;
    scr1Off &= 0xFFC000;
    
    u32 scr2Off = ((static_cast<u32>(m_cpsRegs[0x04]) << 8) | m_cpsRegs[0x05]) << 8;
    scr2Off &= 0xFFC000;
    
    u32 scr3Off = ((static_cast<u32>(m_cpsRegs[0x06]) << 8) | m_cpsRegs[0x07]) << 8;
    scr3Off &= 0xFFC000;
    
    // Get scroll coordinates
    s32 scr1X = static_cast<s16>((static_cast<u16>(m_cpsRegs[0x0C]) << 8) | m_cpsRegs[0x0D]);
    s32 scr1Y = static_cast<s16>((static_cast<u16>(m_cpsRegs[0x0E]) << 8) | m_cpsRegs[0x0F]);
    
    s32 scr2X = static_cast<s16>((static_cast<u16>(m_cpsRegs[0x10]) << 8) | m_cpsRegs[0x11]);
    s32 scr2Y = static_cast<s16>((static_cast<u16>(m_cpsRegs[0x12]) << 8) | m_cpsRegs[0x13]);
    
    s32 scr3X = static_cast<s16>((static_cast<u16>(m_cpsRegs[0x14]) << 8) | m_cpsRegs[0x15]);
    s32 scr3Y = static_cast<s16>((static_cast<u16>(m_cpsRegs[0x16]) << 8) | m_cpsRegs[0x17]);
    
    // Apply scroll offsets
    scr1X += 0x40 - m_globalXOffs + m_layer1XOffs;
    scr1Y += 0x10 - m_globalYOffs + m_layer1YOffs;
    
    scr2X += 0x40 - m_globalXOffs + m_layer2XOffs;
    scr2Y += 0x10 - m_globalYOffs + m_layer2YOffs;
    
    scr3X += 0x40 - m_globalXOffs + m_layer3XOffs;
    scr3Y += 0x10 - m_globalYOffs + m_layer3YOffs;
    
    // Find VRAM pointers
    u8* scr1Base = findGfxRam(scr1Off, 0x4000);
    u8* scr2Base = findGfxRam(scr2Off, 0x4000);
    u8* scr3Base = findGfxRam(scr3Off, 0x4000);
    
    // Render star fields if enabled (CpsLayEn[4] and CpsLayEn[5])
    // Check enable bits from layer control register
    if (layerCtrl & m_boardConfig.layerEnable[4]) {
        renderStarField(0);  // Star field layer 0
    }
    if (layerCtrl & m_boardConfig.layerEnable[5]) {
        renderStarField(1);  // Star field layer 1
    }
    
    // Render layers from bottom to top
    for (int i = 3; i >= 0; i--) {
        s32 n = draw[i];
        
        switch (n) {
            case 0:  // Sprites
                if (drawMask & 1) {
                    renderSpritesCPS1();
                }
                // TODO: BgHi masking - render next layer with masking enabled
                break;
                
            case 1:  // Scroll 1 (8x8 tiles)
                if ((drawMask & 2) && scr1Base) {
                    renderScroll1CPS1(scr1Base, scr1X, scr1Y);
                }
                break;
                
            case 2:  // Scroll 2 (16x16 tiles)
                if ((drawMask & 4) && scr2Base) {
                    renderScroll2(scr2Base, scr2X, scr2Y);
                }
                break;
                
            case 3:  // Scroll 3 (32x32 tiles)
                if ((drawMask & 8) && scr3Base) {
                    renderScroll3CPS1(scr3Base, scr3X, scr3Y);
                }
                break;
        }
    }
}

void PPU::renderLayersCPS2() {
    /*
     * CPS2 Raster-Based Priority Rendering System
     * ============================================
     * 
     * Supports multiple raster zones, each with its own:
     * - Layer priority configuration
     * - Scroll coordinates
     * - Layer enable bits
     * - Layer-sprite priority register
     * 
     * Rendering process:
     * 1. For each raster zone (nSlice):
     *    - Read layer priorities and enable bits
     *    - Read layer-sprite priorities from Frg[4-5]
     *    - Calculate scanline range (nRasterline[nSlice] to nRasterline[nSlice+1])
     * 
     * 2. For each sprite priority level (0-7):
     *    - For each raster zone:
     *      - Render layers at this priority level
     *      - Interleave sprites between layers based on priority
     */
    
    // Initialize Z-buffer for CPS2 sprite rendering
    initCPS2ZBuffer();
    
    // Ensure zone 0 has current register state
    // (If no raster interrupts occurred, use current registers)
    copyRegistersToZone(0);
    copyFrgRegistersToZone(0);
    
    // Arrays to store layer info for each raster zone
    s32 draw[MAX_RASTER][4];      // Layer order for each zone
    s32 prio[MAX_RASTER][4];      // Layer-sprite priority for each zone
    u32 drawMask[MAX_RASTER];     // Layer enable mask for each zone
    
    // Count how many raster zones are active
    s32 numZones = 0;
    do {
        // Get register set for this zone
        const u8* regs = m_rasterRegs[numZones].data();
        const u8* frg = m_rasterFrg[numZones].data();
        
        // Layer priority from register 0x66-0x67
        u16 layerCtrl = (static_cast<u16>(regs[0x66]) << 8) | regs[0x67];
        
        // Determine layer order (3=top, 0=bottom)
        draw[numZones][3] = (layerCtrl >> 12) & 3;  // Top layer
        draw[numZones][2] = (layerCtrl >> 10) & 3;
        draw[numZones][1] = (layerCtrl >> 8) & 3;
        draw[numZones][0] = (layerCtrl >> 6) & 3;   // Bottom layer
        
        // Determine which layers are enabled
        bool layer1Enable = (layerCtrl & 0x02) != 0;
        bool layer2Enable = (layerCtrl & 0x04) != 0;
        bool layer3Enable = (layerCtrl & 0x08) != 0;
        
        // Build enable mask
        drawMask[numZones] = 1;  // Sprites always on
        if (layer1Enable) drawMask[numZones] |= 2;
        if (layer2Enable) drawMask[numZones] |= 4;
        if (layer3Enable) drawMask[numZones] |= 8;
        
        // Layer-sprite priority from Frg registers 0x04-0x05 (word at 0x400004)
        u16 layPri = (static_cast<u16>(frg[0x04]) << 8) | frg[0x05];
        prio[numZones][3] = (layPri >> 12) & 7;
        prio[numZones][2] = (layPri >> 8) & 7;
        prio[numZones][1] = (layPri >> 4) & 7;
        prio[numZones][0] = 0;  // Bottom layer always priority 0
        
        // Check for repeated layers (if found, discard the lower one)
        for (int i = 0; i < 3; i++) {
            for (int j = i + 1; j < 4; j++) {
                if (draw[numZones][i] == draw[numZones][j]) {
                    draw[numZones][j] = -1;
                }
            }
        }
        
        // Normalize priorities (lower layers can't have higher priority than upper layers)
        s32 highPrio = 9999;
        for (s32 i = 3; i >= 0; i--) {
            if (draw[numZones][i] > 0) {
                if (prio[numZones][draw[numZones][i]] > highPrio) {
                    prio[numZones][draw[numZones][i]] = highPrio;
                } else {
                    highPrio = prio[numZones][draw[numZones][i]];
                }
            }
        }
        
        numZones++;
    } while (numZones < MAX_RASTER && m_rasterLines[numZones] > 0);
    
    // Render sprites and layers by priority level (0=lowest, 7=highest)
    s32 prevPrio = -1;
    for (s32 currPrio = 0; currPrio < 8; currPrio++) {
        // For each raster zone
        s32 nSlice = 0;
        do {
            const u8* regs = m_rasterRegs[nSlice].data();
            
            // For each layer slot (bottom to top)
            for (s32 i = 0; i < 4; i++) {
                s32 layerNum = draw[nSlice][i];
                
                // Check if this layer should render at current priority
                if (layerNum >= 0 && prio[nSlice][layerNum] == currPrio) {
                    // Render sprites between previous layer and this one
                    if ((drawMask[0] & 1) && (prevPrio < currPrio)) {
                        renderSpritesCPS2ByPriority(prevPrio + 1, currPrio);
                        prevPrio = currPrio;
                    }
                    
                    // Get scanline range for this zone
                    s32 startLine = m_rasterLines[nSlice];
                    s32 endLine = m_rasterLines[nSlice + 1];
                    if (endLine == 0) {
                        endLine = SCREEN_HEIGHT;
                    }
                    
                    // Get scroll layer base addresses
                    u32 scr1Off = ((static_cast<u32>(regs[0x02]) << 8) | regs[0x03]) << 8;
                    scr1Off &= 0xFFC000;
                    
                    u32 scr2Off = ((static_cast<u32>(regs[0x04]) << 8) | regs[0x05]) << 8;
                    scr2Off &= 0xFFC000;
                    
                    u32 scr3Off = ((static_cast<u32>(regs[0x06]) << 8) | regs[0x07]) << 8;
                    scr3Off &= 0xFFC000;
                    
                    // Get scroll coordinates
                    s32 scr1X = static_cast<s16>((static_cast<u16>(regs[0x0C]) << 8) | regs[0x0D]);
                    s32 scr1Y = static_cast<s16>((static_cast<u16>(regs[0x0E]) << 8) | regs[0x0F]);
                    
                    s32 scr2X = static_cast<s16>((static_cast<u16>(regs[0x10]) << 8) | regs[0x11]);
                    s32 scr2Y = static_cast<s16>((static_cast<u16>(regs[0x12]) << 8) | regs[0x13]);
                    
                    s32 scr3X = static_cast<s16>((static_cast<u16>(regs[0x14]) << 8) | regs[0x15]);
                    s32 scr3Y = static_cast<s16>((static_cast<u16>(regs[0x16]) << 8) | regs[0x17]);
                    
                    // Apply scroll offsets
                    scr1X += 0x40 - m_globalXOffs + m_layer1XOffs;
                    scr1Y += 0x10 - m_globalYOffs + m_layer1YOffs;
                    
                    scr2X += 0x40 - m_globalXOffs + m_layer2XOffs;
                    scr2Y += 0x10 - m_globalYOffs + m_layer2YOffs;
                    
                    scr3X += 0x40 - m_globalXOffs + m_layer3XOffs;
                    scr3Y += 0x10 - m_globalYOffs + m_layer3YOffs;
                    
                    // Render the appropriate layer
                    switch (layerNum) {
                        case 1:  // Scroll 1 (8x8 tiles)
                            if (drawMask[nSlice] & 2) {
                                u8* scr1Base = findGfxRam(scr1Off, 0x4000);
                                if (scr1Base) {
                                    renderScroll1CPS2(scr1Base, scr1X, scr1Y, startLine, endLine);
                                }
                            }
                            break;
                            
                        case 2:  // Scroll 2 (16x16 tiles)
                            if (drawMask[nSlice] & 4) {
                                u8* scr2Base = findGfxRam(scr2Off, 0x4000);
                                if (scr2Base) {
                                    // TODO: Support scanline range for Scroll 2
                                    renderScroll2(scr2Base, scr2X, scr2Y);
                                }
                            }
                            break;
                            
                        case 3:  // Scroll 3 (32x32 tiles)
                            if (drawMask[nSlice] & 8) {
                                u8* scr3Base = findGfxRam(scr3Off, 0x4000);
                                if (scr3Base) {
                                    renderScroll3CPS2(scr3Base, scr3X, scr3Y, startLine, endLine);
                                }
                            }
                            break;
                    }
                }
            }
            
            nSlice++;
        } while (nSlice < numZones);
    }
    
    // Render highest priority sprites
    if ((drawMask[0] & 1) && (prevPrio < 7)) {
        renderSpritesCPS2ByPriority(prevPrio + 1, 7);
    }
}

// ============================================================================
// Scroll 1 (8x8 tiles) - CPS1 Version
// ============================================================================

void PPU::renderScroll1CPS1(const u8* base, s32 scrollX, s32 scrollY) {
    if (!base || m_decodedGfx.empty()) return;
    
    s32 ix = (scrollX >> 3) + 1;
    s32 iy = (scrollY >> 3) + 1;
    s32 sx = 8 - (scrollX & 7);
    s32 sy = 8 - (scrollY & 7);
    
    s32 nXTile = SCREEN_WIDTH >> 3;   // 48 tiles
    s32 nYTile = SCREEN_HEIGHT >> 3;  // 28 tiles
    
    for (s32 y = -1; y < nYTile; y++) {
        for (s32 x = -1; x < nXTile; x++) {
            s32 fx = ix + x;
            s32 fy = iy + y;
            
            // Calculate tile map address
            // Format: ((fy & 0x20) << 8) | ((fx & 0x3F) << 7) | ((fy & 0x1F) << 2)
            u32 p = ((fy & 0x20) << 8) | ((fx & 0x3F) << 7) | ((fy & 0x1F) << 2);
            p &= 0x3FFF;
            
            // Read tile data (2 words: tile number and attributes)
            u16 tileNum = (static_cast<u16>(base[p]) << 8) | base[p + 1];
            u16 attrib = (static_cast<u16>(base[p + 2]) << 8) | base[p + 3];
            
            // Map tile through graphics bank mapper
            s32 t = gfxRomBankMapper(GFXTYPE_SCROLL1, tileNum);
            if (t == -1) continue;
            
            // Calculate tile ROM address (8x8 = 64 bytes per tile in decoded format)
            // 8 rows × 8 bytes/row
            u32 tileAddr = t << 6;
            tileAddr += m_gfxScroll[1];
            
            // Get palette (bits 0-4 of attributes) + base 0x20
            u32 palette = 0x20 | (attrib & 0x1F);
            
            // Get flip flags (bits 5-6 of attributes)
            u32 flip = (attrib >> 5) & 3;
            
            // Calculate screen position
            s32 px = sx + (x << 3);
            s32 py = sy + (y << 3);
            
            // Determine if clipping is needed
            bool clipCheck = (x < 0 || x >= nXTile - 1 || y < 0 || y >= nYTile - 1);
            
            drawTile8x8(px, py, tileAddr, palette, flip, clipCheck, 0);
        }
    }
}

// ============================================================================
// Scroll 2 (16x16 tiles with optional row scroll)
// ============================================================================

void PPU::renderScroll2(const u8* base, s32 scrollX, s32 scrollY) {
    if (!base || m_decodedGfx.empty()) return;

    // Check for row scroll enable (bit 0 of register 0x22)
    u16 rowScrollReg = (static_cast<u16>(m_cpsRegs[0x22]) << 8) | m_cpsRegs[0x23];
    bool rowScrollEnabled = (rowScrollReg & 1) != 0;

    // Initialize row scroll if enabled
    const u16* rowScrollTable = nullptr;
    u32 rowScrollStart = 0;
    if (rowScrollEnabled) {
        // Get row scroll table address from register 0x08
        u32 rowScrollTableAddr = ((static_cast<u32>(m_cpsRegs[0x08]) << 8) | m_cpsRegs[0x09]) << 8;
        rowScrollTableAddr &= 0xFFF800; // Mask to align to 2KB boundary

        // Get the row scroll table from VRAM
        rowScrollTable = reinterpret_cast<const u16*>(findGfxRam(rowScrollTableAddr, 0x0800));

        // Get row scroll start offset from register 0x20
        rowScrollStart = (static_cast<u16>(m_cpsRegs[0x20]) << 8) | m_cpsRegs[0x21];
        rowScrollStart += 16;
    }

    s32 iy = (scrollY >> 4) + 1;
    s32 sy = 16 - (scrollY & 15);

    s32 nXTile = SCREEN_WIDTH >> 4;   // 24 tiles
    s32 nYTile = SCREEN_HEIGHT >> 4;  // 14 tiles

    for (s32 y = -1; y < nYTile; y++) {
        // Calculate row-specific scroll X for row scroll
        s32 scrollX_row = scrollX;
        if (rowScrollEnabled && rowScrollTable) {
            // Calculate screen Y position for this row
            s32 screenY = sy + (y << 4);
            // Add row scroll start offset
            s32 rowIndex = screenY + rowScrollStart;
            // Clamp row index to valid range (0-1023 for 1024 entries)
            rowIndex &= 0x3FF;
            // Get row scroll offset from table (big-endian)
            u16 tableValue = rowScrollTable[rowIndex];
            s16 rowOffset = static_cast<s16>((tableValue << 8) | (tableValue >> 8));
            scrollX_row += rowOffset;
        }

        s32 ix = (scrollX_row >> 4) + 1;
        s32 sx = 16 - (scrollX_row & 15);

        for (s32 x = -1; x < nXTile; x++) {
            s32 fx = ix + x;
            s32 fy = iy + y;
            
            // Calculate tile map address (16x16 tiles)
            // Format: ((fy & 0x30) << 8) | ((fx & 0x3F) << 6) | ((fy & 0x0F) << 2)
            u32 p = ((fy & 0x30) << 8) | ((fx & 0x3F) << 6) | ((fy & 0x0F) << 2);
            p &= 0x3FFF;
            
            // Read tile data
            u16 tileNum = (static_cast<u16>(base[p]) << 8) | base[p + 1];
            u16 attrib = (static_cast<u16>(base[p + 2]) << 8) | base[p + 3];
            
            // Map tile through graphics bank mapper
            s32 t = gfxRomBankMapper(GFXTYPE_SCROLL2, tileNum);
            if (t == -1) continue;
            
            // Calculate tile ROM address (16x16 = 128 bytes per tile, 4bpp)
            u32 tileAddr = t << 7;
            tileAddr += m_gfxScroll[2];
            
            // Get palette (bits 0-4 of attributes) + base 0x40
            u32 palette = 0x40 | (attrib & 0x1F);
            
            // Get flip flags
            u32 flip = (attrib >> 5) & 3;
            
            // Calculate screen position
            s32 px = sx + (x << 4);
            s32 py = sy + (y << 4);
            
            // Determine if clipping is needed
            bool clipCheck = (x < 0 || x >= nXTile - 1 || y < 0 || y >= nYTile - 1);
            
            drawTile16x16(px, py, tileAddr, palette, flip, clipCheck);
        }
    }
}

// ============================================================================
// Scroll 1 (8x8 tiles) - CPS2 Version
// ============================================================================

void PPU::renderScroll1CPS2(const u8* base, s32 scrollX, s32 scrollY, s32 startLine, s32 endLine) {
    if (!base || m_decodedGfx.empty()) return;
    
    // CPS2 supports partial scanline rendering for raster effects
    
    s32 ix = (scrollX >> 3) + 1;
    s32 iy = (scrollY >> 3) + 1;
    s32 sx = 8 - (scrollX & 7);
    s32 sy = 8 - (scrollY & 7);
    
    // Calculate which tiles we need to render based on scanline range
    s32 nFirstY = (startLine + sy) >> 3;
    s32 nLastY = (endLine + sy) >> 3;
    
    s32 nXTile = SCREEN_WIDTH >> 3;   // 48 tiles
    
    for (s32 y = nFirstY - 1; y < nLastY; y++) {
        // Check if this row intersects with our scanline range
        bool clipY = ((y << 3) < startLine) || (((y << 3) + 8) >= endLine);
        
        for (s32 x = -1; x < nXTile; x++) {
            s32 fx = ix + x;
            s32 fy = iy + y;
            
            // Calculate tile map address
            u32 p = ((fy & 0x20) << 8) | ((fx & 0x3F) << 7) | ((fy & 0x1F) << 2);
            p &= 0x3FFF;
            
            // Read tile data
            u16 tileNum = (static_cast<u16>(base[p]) << 8) | base[p + 1];
            u16 attrib = (static_cast<u16>(base[p + 2]) << 8) | base[p + 3];
            
            // CPS2: Direct addressing, no mapper typically (though code path exists)
            s32 t = gfxRomBankMapper(GFXTYPE_SCROLL1, tileNum);
            if (t == -1) continue;
            
            // Calculate tile ROM address
            u32 tileAddr = t << 6;
            tileAddr += m_gfxScroll[1];
            
            // Get palette
            u32 palette = 0x20 | (attrib & 0x1F);
            
            // Get flip flags
            u32 flip = (attrib >> 5) & 3;
            
            // Calculate screen position
            s32 px = sx + (x << 3);
            s32 py = sy + (y << 3);
            
            // Determine if clipping is needed
            bool clipCheck = (x < 0 || x >= nXTile - 1 || clipY);
            
            drawTile8x8(px, py, tileAddr, palette, flip, clipCheck, 0);
        }
    }
}

// ============================================================================
// Scroll 3 (32x32 tiles) - CPS1 Version
// ============================================================================

void PPU::renderScroll3CPS1(const u8* base, s32 scrollX, s32 scrollY) {
    if (!base || m_decodedGfx.empty()) return;
    
    s32 ix = (scrollX >> 5) + 1;
    s32 iy = (scrollY >> 5) + 1;
    s32 sx = 32 - (scrollX & 31);
    s32 sy = 32 - (scrollY & 31);
    
    s32 nXTile = SCREEN_WIDTH >> 5;   // 12 tiles
    s32 nYTile = SCREEN_HEIGHT >> 5;  // 7 tiles
    
    for (s32 y = -1; y < nYTile; y++) {
        for (s32 x = -1; x < nXTile; x++) {
            s32 fx = ix + x;
            s32 fy = iy + y;
            
            // Calculate tile map address (32x32 tiles)
            // Format: ((fy & 0x38) << 8) | ((fx & 0x3F) << 5) | ((fy & 0x07) << 2)
            u32 p = ((fy & 0x38) << 8) | ((fx & 0x3F) << 5) | ((fy & 0x07) << 2);
            p &= 0x3FFF;
            
            // Read tile data
            u16 tileNum = (static_cast<u16>(base[p]) << 8) | base[p + 1];
            u16 attrib = (static_cast<u16>(base[p + 2]) << 8) | base[p + 3];
            
            // Map tile through graphics bank mapper
            s32 t = gfxRomBankMapper(GFXTYPE_SCROLL3, tileNum);
            if (t == -1) continue;
            
            // Calculate tile ROM address (32x32 = 512 bytes per tile, 4bpp)
            u32 tileAddr = t << 9;
            tileAddr += m_gfxScroll[3];
            
            // Get palette (bits 0-4 of attributes) + base 0x60
            u32 palette = 0x60 | (attrib & 0x1F);
            
            // Get flip flags
            u32 flip = (attrib >> 5) & 3;
            
            // Calculate screen position
            s32 px = sx + (x << 5);
            s32 py = sy + (y << 5);
            
            // Determine if clipping is needed
            bool clipCheck = (x < 0 || x >= nXTile - 1 || y < 0 || y >= nYTile - 1);
            
            drawTile32x32(px, py, tileAddr, palette, flip, clipCheck, 0);
        }
    }
}

// ============================================================================
// Scroll 3 (32x32 tiles) - CPS2 Version
// ============================================================================

void PPU::renderScroll3CPS2(const u8* base, s32 scrollX, s32 scrollY, s32 startLine, s32 endLine) {
    if (!base || m_decodedGfx.empty()) return;
    
    // CPS2 supports partial scanline rendering for raster effects
    
    s32 ix = (scrollX >> 5) + 1;
    s32 iy = (scrollY >> 5) + 1;
    s32 sx = 32 - (scrollX & 31);
    s32 sy = 32 - (scrollY & 31);
    
    // Calculate which tiles we need to render based on scanline range
    s32 nFirstY = (startLine + sy) >> 5;
    s32 nLastY = (endLine + sy) >> 5;
    
    s32 nXTile = SCREEN_WIDTH >> 5;   // 12 tiles
    
    for (s32 y = nFirstY - 1; y < nLastY; y++) {
        // Check if this row intersects with our scanline range
        bool clipY = ((y << 5) < startLine) || (((y << 5) + 32) >= endLine);
        
        for (s32 x = -1; x < nXTile; x++) {
            s32 fx = ix + x;
            s32 fy = iy + y;
            
            // Calculate tile map address
            u32 p = ((fy & 0x38) << 8) | ((fx & 0x3F) << 5) | ((fy & 0x07) << 2);
            p &= 0x3FFF;
            
            // Read tile data
            u16 tileNum = (static_cast<u16>(base[p]) << 8) | base[p + 1];
            u16 attrib = (static_cast<u16>(base[p + 2]) << 8) | base[p + 3];
            
            // CPS2 special tile offset hacks for some games (from FBNeo)
            // TODO: Detect game and apply appropriate hack
            // if (Xmcota && tileNum >= 0x5800) tileNum -= 0x4000;
            // if (Ssf2t && tileNum < 0x5600) tileNum += 0x4000;
            
            // CPS2: Direct addressing, no mapper typically
            s32 t = gfxRomBankMapper(GFXTYPE_SCROLL3, tileNum);
            if (t == -1) continue;
            
            // Calculate tile ROM address
            u32 tileAddr = t << 9;
            tileAddr += m_gfxScroll[3];
            
            // Get palette
            u32 palette = 0x60 | (attrib & 0x1F);
            
            // Get flip flags
            u32 flip = (attrib >> 5) & 3;
            
            // Calculate screen position
            s32 px = sx + (x << 5);
            s32 py = sy + (y << 5);
            
            // Determine if clipping is needed
            bool clipCheck = (x < 0 || x >= nXTile - 1 || clipY);
            
            drawTile32x32(px, py, tileAddr, palette, flip, clipCheck, 0);
        }
    }
}

// ============================================================================
// Star Field Rendering (CPS1 Only)
// ============================================================================

void PPU::renderStarField(s32 layer) {
    // Star field rendering based on FBNeo DrawStar()
    // Each star field layer has 0x1000 bytes of data
    // Position is calculated based on star control register and frame counter
    
    if (layer < 0 || layer > 1) return;
    
    const u8* starData = m_starField.data() + (layer << 12);  // 0x1000 per layer
    
    // Get star control offsets from registers 0x18-0x1F
    // Star 1: regs 0x18-0x19 (X), 0x1A-0x1B (Y)
    // Star 2: regs 0x1C-0x1D (X), 0x1E-0x1F (Y)
    s16 starXOffs = static_cast<s16>((static_cast<u16>(m_cpsRegs[0x18 + (layer << 2)]) << 8) | 
                                     m_cpsRegs[0x19 + (layer << 2)]);
    s16 starYOffs = static_cast<s16>((static_cast<u16>(m_cpsRegs[0x1A + (layer << 2)]) << 8) | 
                                     m_cpsRegs[0x1B + (layer << 2)]);
    
    // Render each star
    for (u32 nStar = 0; nStar < 0x1000; nStar++) {
        u8 starColor = starData[nStar];
        
        // Skip if star is transparent (0x0F)
        if (starColor == 0x0F) continue;
        
        // Calculate star position
        // X: ((nStar >> 8) << 5) - starXOffs + (starColor & 0x1F) - 64
        s32 starX = (((nStar >> 8) << 5) - starXOffs + (starColor & 0x1F) - 64) & 0x01FF;
        
        // Y: (nStar & 0xFF) - starYOffs - 16
        s32 starY = ((nStar & 0xFF) - starYOffs - 16) & 0xFF;
        
        // Check if star is visible on screen
        if (starX >= 0 && starX < SCREEN_WIDTH && starY >= 0 && starY < SCREEN_HEIGHT) {
            // Calculate star palette color
            // Color index = ((starColor & 0xE0) >> 1) + frame_animation
            // Frame animation cycles based on whether bit 7 is set
            u32 baseColor = (starColor & 0xE0) >> 1;
            
            // Simple frame animation (would need actual frame counter)
            // For now, use scanline as pseudo-frame counter
            u32 frameAnim = (m_scanline >> 4) % ((starColor & 0x80) ? 0x0E : 0x0F);
            
            u32 colorIndex = baseColor + frameAnim;
            
            // Stars use palette base 0x0800 + (layer << 9)
            u32 paletteIndex = 0x0800 + (layer << 9) + colorIndex;
            
            if (paletteIndex < m_palette.size()) {
                plotPixel(starX, starY, m_palette[paletteIndex]);
            }
        }
    }
}

// ============================================================================
// Sprite Rendering - CPS1 Version
// ============================================================================

void PPU::renderSpritesCPS1() {
    if (m_decodedGfx.empty()) return;
    
    // CPS1: Get sprite table address from register 0x00-0x01
    u32 sprOff = ((static_cast<u32>(m_cpsRegs[0x00]) << 8) | m_cpsRegs[0x01]) << 8;
    sprOff &= 0xFFF800;
    
    u8* sprBase = findGfxRam(sprOff, 0x800);
    if (!sprBase) return;
    
    // CPS1 has 256 sprites maximum, each 8 bytes
    // Sprite format:
    // Word 0 (bytes 0-1): X position (9-bit signed)
    // Word 1 (bytes 2-3): Y position (9-bit signed), high bits are extra tile bits
    // Word 2 (bytes 4-5): Tile number
    // Word 3 (bytes 6-7): Attributes (palette, flip, size)
    
    // Find where the sprite list ends
    s32 spriteEnd = 256;
    for (s32 i = 0; i < 256; i++) {
        u8* ps = sprBase + (i * 8);
        u16 attrib = (static_cast<u16>(ps[6]) << 8) | ps[7];
        
        // Check for end of sprite list (attrib >= 0xFF00)
        if (attrib >= 0xFF00) {
            spriteEnd = i;
            break;
        }
    }
    
    // Render sprites in reverse order (CPS1 default, unless CpsDrawSpritesInReverse)
    for (s32 i = spriteEnd - 1; i >= 0; i--) {
        u8* ps = sprBase + (i * 8);
        
        u16 xData = (static_cast<u16>(ps[0]) << 8) | ps[1];
        u16 yData = (static_cast<u16>(ps[2]) << 8) | ps[3];
        u16 tileNum = (static_cast<u16>(ps[4]) << 8) | ps[5];
        u16 attrib = (static_cast<u16>(ps[6]) << 8) | ps[7];
        
        // Skip blank sprites
        if ((xData | attrib) == 0) continue;
        
        // Get sprite size (in 16x16 blocks)
        s32 bx = ((attrib >> 8) & 15) + 1;
        s32 by = ((attrib >> 12) & 15) + 1;
        
        // Map tile through graphics bank mapper
        s32 n = gfxRomBankMapper(GFXTYPE_SPRITES, tileNum);
        if (n == -1) continue;
        
        // Add high bits from Y data (bits 14-13 become bits 16-15)
        n |= (yData & 0x6000) << 3;
        
        // Get X/Y coordinates (9-bit signed)
        s32 x = xData & 0x01FF;
        if (x >= 0x1C0) x -= 0x200;
        
        s32 y = yData & 0x01FF;
        y ^= 0x100;
        y -= 0x100;
        
        // Apply CPS1 sprite offsets
        x -= 0x40;
        y -= 0x10;
        x += m_globalXOffs;
        y += m_globalYOffs;
        
        // Get palette (bits 0-4)
        u32 palette = attrib & 0x1F;
        
        // Get flip flags (bits 5-6)
        u32 flip = (attrib >> 5) & 3;
        
        // Render all tiles in the sprite
        for (s32 dy = 0; dy < by; dy++) {
            for (s32 dx = 0; dx < bx; dx++) {
                s32 ex, ey;
                
                // Apply flip to tile position within sprite
                if (flip & 1) ex = bx - dx - 1;
                else ex = dx;
                
                if (flip & 2) ey = by - dy - 1;
                else ey = dy;
                
                s32 px = x + (ex << 4);
                s32 py = y + (ey << 4);
                
                // Calculate tile number for this part of sprite (pgear fix)
                s32 tile = (n & ~0x0F) + (dy << 4) + ((n + dx) & 0x0F);
                
                // Calculate tile ROM address (16x16 = 128 bytes per tile)
                u32 tileAddr = tile << 7;
                
                // Check if clipping needed
                bool clipCheck = (px < 0 || py < 0 || 
                                  px + 16 > SCREEN_WIDTH || 
                                  py + 16 > SCREEN_HEIGHT);
                
                drawTile16x16(px, py, tileAddr, palette, flip, clipCheck);
            }
        }
    }
}

// ============================================================================
// Sprite Rendering - CPS2 Version
// ============================================================================

void PPU::renderSpritesCPS2() {
    if (m_decodedGfx.empty()) return;
    
    // Initialize Z-buffer for this frame
    initCPS2ZBuffer();
    
    // Render sprites by priority level (0-7)
    // Level 0 is lowest, level 7 is highest
    s32 prevPrio = -1;
    for (s32 currPrio = 0; currPrio < 8; currPrio++) {
        // Render sprites at this priority level
        renderSpritesCPS2ByPriority(prevPrio + 1, currPrio);
        prevPrio = currPrio;
    }
}

void PPU::renderSpritesCPS2ByPriority(s32 levelFrom, s32 levelTo) {
    if (m_decodedGfx.empty()) return;
    
    // CPS2: Get sprite table from double-buffered object RAM
    // CpsRam708 + ((nCpsObjectBank ^ 1) << 15)
    // Each buffer is 0x8000 (32KB) in size
    u32 objOffset = ((m_objectBank ^ 1) << 15);  // Select inactive buffer for reading
    
    // CPS2 supports up to 1024 sprites (8 bytes each = 8KB max)
    s32 maxSprites = 1024;
    
    // Find where the sprite list ends
    s32 spriteEnd = maxSprites;
    for (s32 i = 0; i < maxSprites; i++) {
        u32 sprAddr = objOffset + (i * 8);
        if (sprAddr + 7 >= 0x10000) break;  // Beyond 64KB object RAM
        
        // Read sprite words (big-endian format) for end detection
        u16 yData = readObjRAM16(sprAddr + 2);
        u16 attrib = readObjRAM16(sprAddr + 6);
        
        // CPS2 end of sprite list: word1 & 0x8000 or attrib >= 0xFF00
        if ((yData & 0x8000) || (attrib >= 0xFF00)) {
            spriteEnd = i;
            break;
        }
    }
    
    // Get sprite offsets from CPS2 Frg registers
    // CpsSaveFrg[0][0x9] = X offset, CpsSaveFrg[0][0xB] = Y offset
    s16 sprXOffset = -(s8)readFrgReg8(0x09) + m_globalXOffs;
    s16 sprYOffset = -(s8)readFrgReg8(0x0B) + m_globalYOffs;
    
    // Iterate through sprites
    // Sprites are processed in order (not reversed like CPS1)
    m_currentZValue = static_cast<u16>(m_maxZValue);
    
    for (s32 i = 0; i < spriteEnd; i++) {
        u32 sprAddr = objOffset + (i * 8);
        
        // Read sprite data from object RAM (big-endian format)
        u16 xData = readObjRAM16(sprAddr + 0);
        u16 yData = readObjRAM16(sprAddr + 2);
        u16 tileNum = readObjRAM16(sprAddr + 4);
        u16 attrib = readObjRAM16(sprAddr + 6);
        
        // Get sprite priority from bits [15:13] of xData
        u32 priority = (xData >> 13) & 7;
        
        // Check if sprite priority is in our rendering range
        if (static_cast<s32>(priority) < levelFrom || static_cast<s32>(priority) > levelTo) {
            m_currentZValue++;
            continue;
        }
        
        // Check if this priority level is enabled
        if ((m_spriteEnableMask & (1 << priority)) == 0) {
            m_currentZValue++;
            continue;
        }
        
        // Skip blank sprites
        if ((xData | attrib) == 0) {
            m_currentZValue++;
            continue;
        }
        
        // Update Z-buffer tracking
        m_maxZValue = m_currentZValue;
        
        // Get sprite size
        s32 bx = ((attrib >> 8) & 15) + 1;
        s32 by = ((attrib >> 12) & 15) + 1;
        
        // CPS2: Direct tile addressing
        s32 n = tileNum;
        
        // Add high bits from Y data (bits 14-13 become bits 16-15)
        n |= (yData & 0x6000) << 3;
        
        // Get X/Y coordinates (10-bit signed for CPS2)
        s32 x = xData & 0x03FF;
        x ^= 0x200;
        x -= 0x200;
        
        s32 y = yData & 0x03FF;
        y ^= 0x200;
        y -= 0x200;
        
        // Apply sprite offsets
        x += sprXOffset;
        y += sprYOffset;
        
        // Get palette
        u32 palette = attrib & 0x1F;
        
        // Get flip flags
        u32 flip = (attrib >> 5) & 3;
        
        // Render all tiles in the sprite
        for (s32 dy = 0; dy < by; dy++) {
            for (s32 dx = 0; dx < bx; dx++) {
                s32 ex, ey;
                
                if (flip & 1) ex = bx - dx - 1;
                else ex = dx;
                
                if (flip & 2) ey = by - dy - 1;
                else ey = dy;
                
                s32 px = x + (ex << 4);
                s32 py = y + (ey << 4);
                
                // Calculate tile number (pgear fix)
                s32 tile = (n & ~0x0F) + (dy << 4) + ((n + dx) & 0x0F);
                
                // Calculate tile ROM address
                u32 tileAddr = tile << 7;
                
                // Check if clipping needed
                bool clipCheck = (px < 0 || py < 0 || 
                                  px + 16 > SCREEN_WIDTH || 
                                  py + 16 > SCREEN_HEIGHT);
                
                // Draw with Z-buffer support
                drawTile16x16WithZ(px, py, tileAddr, palette, flip, clipCheck);
            }
        }
        
        m_currentZValue++;
    }
    
    // Update max Z mask
    if (m_maxZValue > m_maxZMask) {
        m_maxZMask = m_maxZValue;
    }
}

// ============================================================================
// Tile Drawing Functions
// ============================================================================

inline void PPU::plotPixel(s32 x, s32 y, u32 color) {
    if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
        m_frameBuffer[y * SCREEN_WIDTH + x] = color;
    }
}

inline void PPU::plotPixelWithZ(s32 x, s32 y, u32 color) {
    if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
        u32 offset = y * SCREEN_WIDTH + x;
        // Only draw if Z-buffer allows (sprite hasn't been drawn here yet at this Z level)
        if (m_zBuffer[offset] < m_currentZValue) {
            m_frameBuffer[offset] = color;
            m_zBuffer[offset] = m_currentZValue;
        }
    }
}

inline bool PPU::isPixelVisible(s32 x, s32 y) {
    return (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT);
}

void PPU::drawTile8x8(s32 x, s32 y, u32 tileAddr, u32 palette, u32 flip, bool clipCheck, u16 mask) {
    // Check bounds
    tileAddr &= m_gfxMask;
    if (tileAddr >= m_gfxLen) return;
    
    const u8* tileData = m_decodedGfx.data() + tileAddr;
    const u32* pal = m_palette.data() + (palette << 4);
    
    // Format: 8-byte stride per row (4 bytes left + 4 bytes right)
    // For 8x8 tiles, only the left half is used
    s32 tileAdd = 8;  // 8 bytes per row in decoded format
    
    if (flip & 2) {
        // Vertical flip
        tileData += 7 * tileAdd;
        tileAdd = -tileAdd;
    }
    
    for (s32 ty = 0; ty < 8; ty++, tileData += tileAdd) {
        s32 py = y + ty;
        
        if (clipCheck && (py < 0 || py >= SCREEN_HEIGHT)) continue;
        
        // Get 8 pixels from decoded graphics (left half only)
        // Format: pixel 0 in bits 28-31, pixel 1 in bits 24-27, etc.
        u32 pix = *reinterpret_cast<const u32*>(tileData);
        
        if (flip & 1) {
            // Horizontal flip - read pixels from right to left
            // Pixel order becomes: 7,6,5,4,3,2,1,0
            for (s32 tx = 0; tx < 8; tx++) {
                s32 px = x + tx;
                // Extract from low nibble first for flipped
                u8 c = (pix >> (tx * 4)) & 0x0F;
                
                // Check mask bit if masking enabled
                bool masked = mask && !(mask & (1 << tx));
                
                if (c && !masked && (!clipCheck || isPixelVisible(px, py))) {
                    plotPixel(px, py, pal[c]);
                }
            }
        } else {
            // Normal - read pixels from left to right
            // Pixel 0 from bits 28-31, pixel 1 from bits 24-27, etc.
            for (s32 tx = 0; tx < 8; tx++) {
                s32 px = x + tx;
                u8 c = (pix >> ((7 - tx) * 4)) & 0x0F;
                
                // Check mask bit if masking enabled
                bool masked = mask && !(mask & (1 << tx));
                
                if (c && !masked && (!clipCheck || isPixelVisible(px, py))) {
                    plotPixel(px, py, pal[c]);
                }
            }
        }
    }
}

void PPU::drawTile16x16(s32 x, s32 y, u32 tileAddr, u32 palette, u32 flip, bool clipCheck) {
    tileAddr &= m_gfxMask;
    if (tileAddr >= m_gfxLen) return;
    
    const u8* tileData = m_decodedGfx.data() + tileAddr;
    const u32* pal = m_palette.data() + (palette << 4);
    
    s32 tileAdd = 8;  // Bytes per row (16 pixels = two 32-bit words)
    
    if (flip & 2) {
        // Vertical flip
        tileData += 15 * tileAdd;
        tileAdd = -tileAdd;
    }
    
    for (s32 ty = 0; ty < 16; ty++, tileData += tileAdd) {
        s32 py = y + ty;
        
        if (clipCheck && (py < 0 || py >= SCREEN_HEIGHT)) continue;
        
        // Get 16 pixels (two 32-bit words)
        const u32* pixData = reinterpret_cast<const u32*>(tileData);
        
        if (flip & 1) {
            // Horizontal flip - swap word order and reverse pixel order
            u32 pix1 = pixData[1];  // Second word drawn first
            u32 pix0 = pixData[0];
            
            // Draw second word's pixels reversed (pixels 8-15 become 0-7)
            for (s32 tx = 0; tx < 8; tx++) {
                s32 px = x + tx;
                u8 c = (pix1 >> (tx * 4)) & 0x0F;
                
                if (c && (!clipCheck || isPixelVisible(px, py))) {
                    plotPixel(px, py, pal[c]);
                }
            }
            // Draw first word's pixels reversed
            for (s32 tx = 0; tx < 8; tx++) {
                s32 px = x + 8 + tx;
                u8 c = (pix0 >> (tx * 4)) & 0x0F;
                
                if (c && (!clipCheck || isPixelVisible(px, py))) {
                    plotPixel(px, py, pal[c]);
                }
            }
        } else {
            // Normal - pixels in order
            u32 pix0 = pixData[0];
            u32 pix1 = pixData[1];
            
            // Draw first word (pixels 0-7)
            for (s32 tx = 0; tx < 8; tx++) {
                s32 px = x + tx;
                u8 c = (pix0 >> ((7 - tx) * 4)) & 0x0F;
                
                if (c && (!clipCheck || isPixelVisible(px, py))) {
                    plotPixel(px, py, pal[c]);
                }
            }
            // Draw second word (pixels 8-15)
            for (s32 tx = 0; tx < 8; tx++) {
                s32 px = x + 8 + tx;
                u8 c = (pix1 >> ((7 - tx) * 4)) & 0x0F;
                
                if (c && (!clipCheck || isPixelVisible(px, py))) {
                    plotPixel(px, py, pal[c]);
                }
            }
        }
    }
}

void PPU::drawTile16x16WithZ(s32 x, s32 y, u32 tileAddr, u32 palette, u32 flip, bool clipCheck) {
    // Same as drawTile16x16 but uses Z-buffer for depth testing
    tileAddr &= m_gfxMask;
    if (tileAddr >= m_gfxLen) return;
    
    const u8* tileData = m_decodedGfx.data() + tileAddr;
    const u32* pal = m_palette.data() + (palette << 4);
    
    s32 tileAdd = 8;
    
    if (flip & 2) {
        tileData += 15 * tileAdd;
        tileAdd = -tileAdd;
    }
    
    for (s32 ty = 0; ty < 16; ty++, tileData += tileAdd) {
        s32 py = y + ty;
        
        if (clipCheck && (py < 0 || py >= SCREEN_HEIGHT)) continue;
        
        const u32* pixData = reinterpret_cast<const u32*>(tileData);
        
        if (flip & 1) {
            u32 pix1 = pixData[1];
            u32 pix0 = pixData[0];
            
            for (s32 tx = 0; tx < 8; tx++) {
                s32 px = x + tx;
                u8 c = (pix1 >> (tx * 4)) & 0x0F;
                
                if (c && (!clipCheck || isPixelVisible(px, py))) {
                    plotPixelWithZ(px, py, pal[c]);
                }
            }
            for (s32 tx = 0; tx < 8; tx++) {
                s32 px = x + 8 + tx;
                u8 c = (pix0 >> (tx * 4)) & 0x0F;
                
                if (c && (!clipCheck || isPixelVisible(px, py))) {
                    plotPixelWithZ(px, py, pal[c]);
                }
            }
        } else {
            u32 pix0 = pixData[0];
            u32 pix1 = pixData[1];
            
            for (s32 tx = 0; tx < 8; tx++) {
                s32 px = x + tx;
                u8 c = (pix0 >> ((7 - tx) * 4)) & 0x0F;
                
                if (c && (!clipCheck || isPixelVisible(px, py))) {
                    plotPixelWithZ(px, py, pal[c]);
                }
            }
            for (s32 tx = 0; tx < 8; tx++) {
                s32 px = x + 8 + tx;
                u8 c = (pix1 >> ((7 - tx) * 4)) & 0x0F;
                
                if (c && (!clipCheck || isPixelVisible(px, py))) {
                    plotPixelWithZ(px, py, pal[c]);
                }
            }
        }
    }
}

void PPU::drawTile32x32(s32 x, s32 y, u32 tileAddr, u32 palette, u32 flip, bool clipCheck, u16 mask) {
    tileAddr &= m_gfxMask;
    if (tileAddr >= m_gfxLen) return;
    
    const u8* tileData = m_decodedGfx.data() + tileAddr;
    const u32* pal = m_palette.data() + (palette << 4);
    
    s32 tileAdd = 16;  // Bytes per row (32 pixels = four 32-bit words)
    
    if (flip & 2) {
        // Vertical flip
        tileData += 31 * tileAdd;
        tileAdd = -tileAdd;
    }
    
    for (s32 ty = 0; ty < 32; ty++, tileData += tileAdd) {
        s32 py = y + ty;
        
        if (clipCheck && (py < 0 || py >= SCREEN_HEIGHT)) continue;
        
        // Get 32 pixels (four 32-bit words)
        const u32* pixData = reinterpret_cast<const u32*>(tileData);
        
        if (flip & 1) {
            // Horizontal flip - reverse word order and pixel order
            for (s32 w = 0; w < 4; w++) {
                u32 pix = pixData[3 - w];  // Reverse word order
                for (s32 tx = 0; tx < 8; tx++) {
                    s32 px = x + (w * 8) + tx;
                    // Reverse pixel order within word
                    u8 c = (pix >> (tx * 4)) & 0x0F;
                    
                    // Check mask if enabled
                    s32 pixelIndex = (w * 8) + tx;
                    bool masked = mask && !(mask & (1 << pixelIndex));
                    
                    if (c && !masked && (!clipCheck || isPixelVisible(px, py))) {
                        plotPixel(px, py, pal[c]);
                    }
                }
            }
        } else {
            // Normal - pixels in order
            for (s32 w = 0; w < 4; w++) {
                u32 pix = pixData[w];
                for (s32 tx = 0; tx < 8; tx++) {
                    s32 px = x + (w * 8) + tx;
                    u8 c = (pix >> ((7 - tx) * 4)) & 0x0F;
                    
                    // Check mask if enabled
                    s32 pixelIndex = (w * 8) + tx;
                    bool masked = mask && !(mask & (1 << pixelIndex));
                    
                    if (c && !masked && (!clipCheck || isPixelVisible(px, py))) {
                        plotPixel(px, py, pal[c]);
                    }
                }
            }
        }
    }
}

// ============================================================================
// Save/Load State
// ============================================================================

void PPU::saveState(std::ofstream& file) {
    // Save VRAM
    file.write(reinterpret_cast<const char*>(m_vram.data()), m_vram.size());
    
    // Save registers
    file.write(reinterpret_cast<const char*>(m_cpsRegs.data()), m_cpsRegs.size());
    
    // Save scroll offsets
    file.write(reinterpret_cast<const char*>(&m_layer1XOffs), sizeof(m_layer1XOffs));
    file.write(reinterpret_cast<const char*>(&m_layer1YOffs), sizeof(m_layer1YOffs));
    file.write(reinterpret_cast<const char*>(&m_layer2XOffs), sizeof(m_layer2XOffs));
    file.write(reinterpret_cast<const char*>(&m_layer2YOffs), sizeof(m_layer2YOffs));
    file.write(reinterpret_cast<const char*>(&m_layer3XOffs), sizeof(m_layer3XOffs));
    file.write(reinterpret_cast<const char*>(&m_layer3YOffs), sizeof(m_layer3YOffs));
    file.write(reinterpret_cast<const char*>(&m_globalXOffs), sizeof(m_globalXOffs));
    file.write(reinterpret_cast<const char*>(&m_globalYOffs), sizeof(m_globalYOffs));
    
    // Save graphics scroll offsets
    file.write(reinterpret_cast<const char*>(m_gfxScroll), sizeof(m_gfxScroll));
    
    // Save state flags
    file.write(reinterpret_cast<const char*>(&m_frameComplete), sizeof(m_frameComplete));
    file.write(reinterpret_cast<const char*>(&m_scanline), sizeof(m_scanline));
    file.write(reinterpret_cast<const char*>(&m_cycles), sizeof(m_cycles));
    file.write(reinterpret_cast<const char*>(&m_paletteNeedsUpdate), sizeof(m_paletteNeedsUpdate));
}

void PPU::loadState(std::ifstream& file) {
    // Load VRAM
    file.read(reinterpret_cast<char*>(m_vram.data()), m_vram.size());
    
    // Load registers
    file.read(reinterpret_cast<char*>(m_cpsRegs.data()), m_cpsRegs.size());
    
    // Load scroll offsets
    file.read(reinterpret_cast<char*>(&m_layer1XOffs), sizeof(m_layer1XOffs));
    file.read(reinterpret_cast<char*>(&m_layer1YOffs), sizeof(m_layer1YOffs));
    file.read(reinterpret_cast<char*>(&m_layer2XOffs), sizeof(m_layer2XOffs));
    file.read(reinterpret_cast<char*>(&m_layer2YOffs), sizeof(m_layer2YOffs));
    file.read(reinterpret_cast<char*>(&m_layer3XOffs), sizeof(m_layer3XOffs));
    file.read(reinterpret_cast<char*>(&m_layer3YOffs), sizeof(m_layer3YOffs));
    file.read(reinterpret_cast<char*>(&m_globalXOffs), sizeof(m_globalXOffs));
    file.read(reinterpret_cast<char*>(&m_globalYOffs), sizeof(m_globalYOffs));
    
    // Load graphics scroll offsets
    file.read(reinterpret_cast<char*>(m_gfxScroll), sizeof(m_gfxScroll));
    
    // Load state flags
    file.read(reinterpret_cast<char*>(&m_frameComplete), sizeof(m_frameComplete));
    file.read(reinterpret_cast<char*>(&m_scanline), sizeof(m_scanline));
    file.read(reinterpret_cast<char*>(&m_cycles), sizeof(m_cycles));
    file.read(reinterpret_cast<char*>(&m_paletteNeedsUpdate), sizeof(m_paletteNeedsUpdate));
    
    // Force palette update after loading state
    m_paletteNeedsUpdate = true;
}

// ============================================================================
// IMPLEMENTATION STATUS AND TODO
// ============================================================================

/*
 * IMPLEMENTED FEATURES:
 * ====================
 * 
 * 1. Documentation:
 *    ✓ Comprehensive header documentation explaining CPS1 vs CPS2 differences
 *    ✓ Documented all major rendering differences from FBNeo reference
 *    ✓ Clear function separation between CPS1 and CPS2
 * 
 * 2. Screen Clearing:
 *    ✓ CPS1: Clears to palette entry 0xBFF ^ 15 (background color)
 *    ✓ CPS2: Clears to black (memset to 0)
 * 
 * 3. Palette Handling:
 *    ✓ CPS1: Always updates all 6 palette pages
 *    ✓ CPS2: Only updates pages enabled in palette control register
 *    ✓ Both use same 16-bit format with brightness
 * 
 * 4. Layer Priority System:
 *    ✓ CPS1: renderLayersCPS1() - simple priority from layer control register
 *    ✓ CPS2: renderLayersCPS2() - priority-based rendering with sprite levels
 * 
 * 5. Scroll Layer Rendering:
 *    ✓ CPS1: renderScroll1CPS1() and renderScroll3CPS1() - full screen
 *    ✓ CPS2: renderScroll1CPS2() and renderScroll3CPS2() - scanline-based
 *    ✓ Scroll 2 is common (row scroll works on both)
 *    ✓ Tile masking support (BgHi mode for CPS1)
 * 
 * 6. Sprite Rendering:
 *    ✓ CPS1: renderSpritesCPS1() - 256 sprites, reverse order, blending support
 *    ✓ CPS2: renderSpritesCPS2() - 1024 sprites, 8-level priority system
 *    ✓ Z-buffer implementation for CPS2 sprite depth sorting
 *    ✓ Sprite blending framework (blend table loading infrastructure)
 * 
 * 7. CPS2 Z-Buffer System:
 *    ✓ Z-buffer allocation (SCREEN_WIDTH * SCREEN_HEIGHT * 2 bytes)
 *    ✓ initCPS2ZBuffer() to manage Z-buffer initialization
 *    ✓ Track nMaxZValue, nMaxZMask, nZOffset
 *    ✓ Clear Z-buffer when overflow detected (>= 0xFC00)
 *    ✓ Render sprites by priority level (0-7) with renderSpritesCPS2ByPriority()
 *    ✓ plotPixelWithZ() for Z-buffer depth testing
 *    ✓ drawTile16x16WithZ() for sprites with Z-buffer support
 * 
 * 8. Star Field (CPS1):
 *    ✓ renderStarField() implementation
 *    ✓ Support for 2 star field layers (CpsLayEn[4] and [5])
 *    ✓ Star position calculation based on control registers
 *    ✓ Frame-based star animation
 *    ✓ Integration into CPS1 layer rendering
 * 
 * 9. Tile Masking:
 *     ✓ BgHi masking infrastructure for CPS1
 *     ✓ drawTile8x8() supports mask parameter
 *     ✓ drawTile32x32() supports mask parameter
 *     ✓ m_currentMask, m_bgHiMode, m_maskAddr state tracking
 * 
 * 
 * REMAINING TODO (Advanced Features):
 * ===================================
 * 
 * 1. Raster Interrupt System (CPS2):
 *    - Implement full MAX_RASTER zone support (currently single zone)
 *    - Support multiple register sets per scanline (m_rasterRegs[])
 *    - Track scanline boundaries dynamically (m_rasterLines[])
 *    - Allow mid-frame scroll position and priority changes
 *    - Integrate with CPS2 layer rendering
 * 
 * 2. CPS2 Sprite Advanced Features:
 *    - Access double-buffered sprite RAM (CpsRam708 + offset)
 *    - Get sprite offsets from CpsSaveFrg[0][0x9] and [0xB]
 *    - Implement layer-sprite priority register (CpsSaveFrg[4-5])
 *    - Add Marvel vs Capcom offset hack (attrib & 0x80)
 * 
 * 3. Game-Specific Hacks (SKIPPED FOR NOW):
 *    - Scroll 3 tile offset for Xmcota (tile >= 0x5800 -> subtract 0x4000)
 *    - Scroll 3 tile offset for Ssf2t (tile < 0x5600 -> add 0x4000)
 *    - Cps2Turbo tile addressing adjustment
 *    - SFA2 high score screen hack
 * 
 * 
 * 5. CPS1 BgHi Masking (Full Implementation):
 *    - Read mask values from MaskAddr[] registers
 *    - Apply masking based on tile attributes
 *    - Support drawing scroll layers over sprites with masking
 *    - Integrate into layer rendering order
 * 
 * 6. Additional Features:
 *    - CpsDrawSpritesInReverse flag support
 *    - Sprite list detection improvements (Cps1DetectEndSpriteList8000)
 *    - Bootleg sprite RAM support (various bootlegs)
 *    - Custom callbacks (Cps1ObjGetCallback, Cps1ObjDrawCallback)
 * 
 * 
 * REFERENCE FILES:
 * ================
 * - ref/FBNeo/src/burn/drv/capcom/cps_draw.cpp  - Layer priority and rendering
 * - ref/FBNeo/src/burn/drv/capcom/cps_scr.cpp   - Scroll layer rendering
 * - ref/FBNeo/src/burn/drv/capcom/cps_obj.cpp   - Sprite rendering and Z-buffer
 * - ref/FBNeo/src/burn/drv/capcom/cps_pal.cpp   - Palette conversion
 */

// ============================================================================
// CPS2 Memory Access Helpers
// ============================================================================

u8 PPU::readObjRAM8(u32 offset) {
    if (!m_memory) return 0;
    // Object RAM is at 0x708000-0x717FFF (64KB)
    if (offset >= 0x10000) return 0;  // 64KB max
    return m_memory->read8(0x708000 + offset);
}

u16 PPU::readObjRAM16(u32 offset) {
    if (!m_memory) return 0;
    if (offset >= 0x10000) return 0;
    return m_memory->read16(0x708000 + offset);
}

u8 PPU::readFrgReg8(u8 reg) {
    if (!m_memory) return 0;
    // Frg registers are at 0x400000-0x40000F (16 bytes)
    if (reg >= 0x10) return 0;
    return m_memory->read8(0x400000 + reg);
}

u16 PPU::readFrgReg16(u8 reg) {
    if (!m_memory) return 0;
    if (reg >= 0x10) return 0;
    return m_memory->read16(0x400000 + reg);
}

// ============================================================================
// CPS2 Raster Interrupt Management
// ============================================================================

void PPU::setRasterLine(u32 zone, s32 scanline) {
    /*
     * Set the scanline boundary for a raster zone.
     * 
     * CPS2 can have multiple raster interrupt zones (up to MAX_RASTER).
     * Each zone renders with its own register set from m_rasterLines[zone]
     * to m_rasterLines[zone+1].
     * 
     * Zone 0 always starts at scanline 0.
     * Setting scanline 0 for zone N>0 disables that zone and all higher zones.
     * 
     * Example:
     *   m_rasterLines[0] = 0     (zone 0 starts)
     *   m_rasterLines[1] = 112   (zone 1 starts, zone 0 ends)
     *   m_rasterLines[2] = 0     (no more zones)
     */
    if (zone >= MAX_RASTER + 2) return;
    m_rasterLines[zone] = scanline;
}

void PPU::copyRegistersToZone(u32 zone) {
    /*
     * Copy current CPS registers to a raster zone's register set.
     * 
     * This is typically called when a raster interrupt fires,
     * to save the current register state for rendering that zone.
     * 
     * The CPU can then modify registers, and the next raster interrupt
     * will save those new values for the next zone.
     */
    if (zone >= MAX_RASTER) return;
    
    // Copy all 256 CPS registers to this zone's register set
    std::copy(m_cpsRegs.begin(), m_cpsRegs.end(), m_rasterRegs[zone].begin());
}

void PPU::copyFrgRegistersToZone(u32 zone) {
    /*
     * Copy current Frg registers to a raster zone's Frg set.
     * 
     * Frg registers (at 0x400000-0x40000F) control layer-sprite priorities
     * and sprite offsets. These can change per raster zone for effects.
     */
    if (zone >= MAX_RASTER) return;
    if (!m_memory) return;
    
    // Copy 16 bytes of Frg registers
    for (u8 i = 0; i < 16; i++) {
        m_rasterFrg[zone][i] = readFrgReg8(i);
    }
}

s32 PPU::getRasterLineCount() const {
    /*
     * Count how many active raster zones exist.
     * 
     * Zones are active until we find a scanline value of 0,
     * or we reach MAX_RASTER zones.
     */
    for (s32 i = 1; i < MAX_RASTER + 2; i++) {
        if (m_rasterLines[i] == 0) {
            return i;
        }
    }
    return MAX_RASTER + 1;
}

} // namespace cps
