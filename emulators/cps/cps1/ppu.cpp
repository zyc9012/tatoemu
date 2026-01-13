#include "ppu.h"
#include "../cartridge.h"
#include "../cpu.h"
#include <cstring>
#include <iostream>
#include <algorithm>

/*
 * CPS1 PPU Implementation
 * =======================
 * 
 * The CPS1 PPU handles rendering of:
 * - 3 scroll layers (background)
 * - 1 sprite layer (objects)
 * - Star field (on some games)
 * 
 * Tile Sizes:
 * - Scroll 1: 8x8 tiles (text layer)
 * - Scroll 2: 16x16 tiles (main background)
 * - Scroll 3: 32x32 tiles (large background)
 * - Sprites: 16x16 tiles (variable size via linking)
 * 
 * Graphics Format:
 * - 4bpp (16 colors per tile)
 * - Stored in planar format in ROM
 * - Needs decoding for efficient rendering
 * 
 * Palette:
 * - 16-bit entries: xxxxBBBB GGGGRRRR with brightness in high bits
 * - 6 pages of 512 colors each (0xC00 entries total)
 * - Brightness factor applied to all RGB channels
 * 
 * References:
 * - FBNeo cps_draw.cpp, cps_scr.cpp, cps_obj.cpp
 * - FBNeo cps_pal.cpp for palette conversion
 * - FBNeo cps.cpp for graphics ROM decoding
 */

namespace cps1 {

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
{
    InitSepTable();
    
    m_frameBuffer.fill(0);
    m_vram.fill(0);
    m_palette.fill(0);
    m_cpsRegs.fill(0);
    
    m_gfxScroll[0] = 0;
    m_gfxScroll[1] = 0;
    m_gfxScroll[2] = 0;
    m_gfxScroll[3] = 0;
    
    m_gfxBankSizes[0] = 0;
    m_gfxBankSizes[1] = 0;
    m_gfxBankSizes[2] = 0;
    m_gfxBankSizes[3] = 0;
}

void PPU::setCartridge(cps::Cartridge* cartridge) {
    m_cartridge = cartridge;
}

void PPU::reset() {
    m_frameBuffer.fill(0);
    m_vram.fill(0);
    m_cpsRegs.fill(0);
    
    m_frameComplete = false;
    m_scanline = 0;
    m_cycles = 0;
    m_paletteNeedsUpdate = true;
    
    // Reset scroll offsets
    m_layer1XOffs = 0; m_layer1YOffs = 0;
    m_layer2XOffs = 0; m_layer2YOffs = 0;
    m_layer3XOffs = 0; m_layer3YOffs = 0;
    m_globalXOffs = 0; m_globalYOffs = 0;
    
    if (m_cartridge) {
        m_boardConfig = m_cartridge->getBoardConfig();
        setupGfxMapper();
        decodeGraphicsROM();
    }
}

void PPU::setupGfxMapper() {
    if (!m_cartridge) return;
    
    cps::CPSMapper mapper = m_cartridge->getMapper();
    
    // Get mapper table and bank sizes from database
    m_gfxMapper = cps::GameDatabase::getGfxMapperTable(mapper);
    if (!m_gfxMapper) {
        throw std::runtime_error("Unsupported mapper");
    }
    
    cps::GameDatabase::getGfxBankSizes(mapper, m_gfxBankSizes);
}

// ============================================================================
// Graphics ROM Decoding
// Converts CPS1 4bpp planar format to linear nibbles for fast rendering
// ============================================================================

void PPU::decodeGraphicsROM() {
    if (!m_cartridge) return;
    
    u32 srcSize = m_cartridge->getGraphicsROMSize();
    if (srcSize == 0) {
        std::cerr << "PPU: No graphics ROM data to decode" << std::endl;
        return;
    }
    
    /*
     * CPS1 Graphics ROM Decoding
     * ==========================================================
     * 
     * CPS1 graphics ROMs are organized as groups of 4 chips (512KB each):
     * - ROM 0: Left half of tiles, bits 0-1
     * - ROM 1: Right half of tiles, bits 0-1
     * - ROM 2: Left half of tiles, bits 2-3
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
     * - Combine ROM0+ROM2 for left pixels, ROM1+ROM3 for right pixels
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
    if (!m_gfxMapper) return code;
    
    s32 shift = 0;
    switch (type) {
        case cps::GFXTYPE_SPRITES: shift = 1; break;
        case cps::GFXTYPE_SCROLL1: shift = 0; break;
        case cps::GFXTYPE_SCROLL2: shift = 1; break;
        case cps::GFXTYPE_SCROLL3: shift = 3; break;
        default: shift = 0; break;
    }
    
    s32 shiftedCode = code << shift;
    
    const cps::GfxRange* range = m_gfxMapper;
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
// Palette Handling
// ============================================================================

u32 PPU::convertPaletteEntry(u16 entry) {
    /*
     * CPS1 palette format (16-bit):
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
    
    // Convert each 16-bit palette entry
    // CPS1 has 6 pages of 512 colors = 3072 entries (0xC00)
    // Each page is 0x400 bytes (512 entries * 2 bytes each)
    for (u32 page = 0; page < 6; page++) {
        // Always update all pages (palette control reg may not be set correctly)
        for (u32 i = 0; i < 0x200; i++) {
            u32 srcOffset = palOffset + (page * 0x400) + (i * 2);
            if (srcOffset + 1 < VRAM_SIZE) {
                // Read 16-bit palette entry (big-endian)
                u16 entry = (static_cast<u16>(m_vram[srcOffset]) << 8) | 
                            m_vram[srcOffset + 1];
                
                // CPS1 palette uses XOR 15 for color indexing
                u32 dstIndex = (page * 0x200) + (i ^ 15);
                if (dstIndex < m_palette.size()) {
                    m_palette[dstIndex] = convertPaletteEntry(entry);
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
    // CPS1 renders a full frame at VBlank
    // The 68000 runs at 10MHz and the frame rate is ~59.63Hz
    // CPU cycles per frame ~= 10000000 / 59.63 = 167706
    
    // Increment cycle counter
    m_cycles++;
    
    // CPS1 typically triggers VBlank interrupt at scanline 224
    // Each scanline takes approximately 167706 / 262 ≈ 640 cycles
    // Total active lines: 224, VBlank: 38 lines (262 total)
    
    constexpr u32 CYCLES_PER_SCANLINE = 640;
    constexpr u32 VISIBLE_SCANLINES = 224;
    constexpr u32 TOTAL_SCANLINES = 262;
    constexpr u32 CYCLES_PER_FRAME = CYCLES_PER_SCANLINE * TOTAL_SCANLINES;
    
    // Calculate current scanline
    u32 newScanline = m_cycles / CYCLES_PER_SCANLINE;
    
    // Check if we've moved to a new scanline
    if (newScanline != m_scanline) {
        m_scanline = newScanline;
        
        // At VBlank start (scanline 224), render the frame and trigger interrupt
        if (m_scanline == VISIBLE_SCANLINES) {
            renderFrame();

            // Trigger VBlank interrupt
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
    // CPS1 clears to palette entry 0xBFF ^ 15
    u32 bgColor = m_palette[0xBFF ^ 15];
    m_frameBuffer.fill(bgColor);
}

// ============================================================================
// Layer Rendering
// ============================================================================

void PPU::renderLayers() {
    // Read layer control register
    u8 lcReg = m_boardConfig.layerControlReg;
    u16 layerCtrl = (static_cast<u16>(m_cpsRegs[lcReg]) << 8) | m_cpsRegs[lcReg + 1];
    
    // Determine which layers are enabled
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
    // Register contains high 16 bits of VRAM address (address / 256)
    // Scroll 1 offset at register 0x02-0x03
    u32 scr1Off = ((static_cast<u32>(m_cpsRegs[0x02]) << 8) | m_cpsRegs[0x03]) << 8;
    scr1Off &= 0xFFC000;
    
    // Scroll 2 offset at register 0x04-0x05
    u32 scr2Off = ((static_cast<u32>(m_cpsRegs[0x04]) << 8) | m_cpsRegs[0x05]) << 8;
    scr2Off &= 0xFFC000;
    
    // Scroll 3 offset at register 0x06-0x07
    u32 scr3Off = ((static_cast<u32>(m_cpsRegs[0x06]) << 8) | m_cpsRegs[0x07]) << 8;
    scr3Off &= 0xFFC000;
    
    // Get scroll coordinates
    // Scroll 1 X/Y at registers 0x0C-0x0F
    s32 scr1X = static_cast<s16>((static_cast<u16>(m_cpsRegs[0x0C]) << 8) | m_cpsRegs[0x0D]);
    s32 scr1Y = static_cast<s16>((static_cast<u16>(m_cpsRegs[0x0E]) << 8) | m_cpsRegs[0x0F]);
    
    // Scroll 2 X/Y at registers 0x10-0x13
    s32 scr2X = static_cast<s16>((static_cast<u16>(m_cpsRegs[0x10]) << 8) | m_cpsRegs[0x11]);
    s32 scr2Y = static_cast<s16>((static_cast<u16>(m_cpsRegs[0x12]) << 8) | m_cpsRegs[0x13]);
    
    // Scroll 3 X/Y at registers 0x14-0x17
    s32 scr3X = static_cast<s16>((static_cast<u16>(m_cpsRegs[0x14]) << 8) | m_cpsRegs[0x15]);
    s32 scr3Y = static_cast<s16>((static_cast<u16>(m_cpsRegs[0x16]) << 8) | m_cpsRegs[0x17]);
    
    // Apply scroll offsets
    scr1X += 0x40 - m_globalXOffs + m_layer1XOffs;
    scr1Y += 0x10 - m_globalYOffs + m_layer1YOffs;
    
    scr2X += 0x40 - m_globalXOffs + m_layer2XOffs;
    scr2Y += 0x10 - m_globalYOffs + m_layer2YOffs;
    
    scr3X += 0x40 - m_globalXOffs + m_layer3XOffs;
    scr3Y += 0x10 - m_globalYOffs + m_layer3YOffs;
    
    // Find VRAM pointers (scr*Off already contains full 24-bit VRAM address)
    u8* scr1Base = findGfxRam(scr1Off, 0x4000);
    u8* scr2Base = findGfxRam(scr2Off, 0x4000);
    u8* scr3Base = findGfxRam(scr3Off, 0x4000);
    
    // Render layers from bottom to top
    for (int i = 3; i >= 0; i--) {
        s32 n = draw[i];
        
        switch (n) {
            case 0:  // Sprites
                if (drawMask & 1) {
                    renderSprites();
                }
                break;
                
            case 1:  // Scroll 1 (8x8 tiles)
                if ((drawMask & 2) && scr1Base) {
                    renderScroll1(scr1Base, scr1X, scr1Y);
                }
                break;
                
            case 2:  // Scroll 2 (16x16 tiles)
                if ((drawMask & 4) && scr2Base) {
                    renderScroll2(scr2Base, scr2X, scr2Y);
                }
                break;
                
            case 3:  // Scroll 3 (32x32 tiles)
                if ((drawMask & 8) && scr3Base) {
                    renderScroll3(scr3Base, scr3X, scr3Y);
                }
                break;
        }
    }
}

// ============================================================================
// Scroll 1 (8x8 tiles)
// ============================================================================

void PPU::renderScroll1(const u8* base, s32 scrollX, s32 scrollY) {
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
            s32 t = gfxRomBankMapper(cps::GFXTYPE_SCROLL1, tileNum);
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
            
            drawTile8x8(px, py, tileAddr, palette, flip, clipCheck);
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
            s32 t = gfxRomBankMapper(cps::GFXTYPE_SCROLL2, tileNum);
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
// Scroll 3 (32x32 tiles)
// ============================================================================

void PPU::renderScroll3(const u8* base, s32 scrollX, s32 scrollY) {
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
            s32 t = gfxRomBankMapper(cps::GFXTYPE_SCROLL3, tileNum);
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
            
            drawTile32x32(px, py, tileAddr, palette, flip, clipCheck);
        }
    }
}

// ============================================================================
// Sprite Rendering
// ============================================================================

void PPU::renderSprites() {
    if (m_decodedGfx.empty()) return;
    
    // Get sprite table address from register 0x00-0x01
    u32 sprOff = ((static_cast<u32>(m_cpsRegs[0x00]) << 8) | m_cpsRegs[0x01]) << 8;
    sprOff &= 0xFFF800;
    
    u8* sprBase = findGfxRam(sprOff, 0x800);
    if (!sprBase) return;
    
    // CPS1 has 256 sprites, each 8 bytes
    // Sprite format:
    // Word 0 (bytes 0-1): X position (9-bit signed)
    // Word 1 (bytes 2-3): Y position (9-bit signed), high bits are extra tile bits
    // Word 2 (bytes 4-5): Tile number
    // Word 3 (bytes 6-7): Attributes (palette, flip, size)
    
    // First, find where the sprite list ends by iterating forward
    s32 spriteEnd = 256;  // Default to end of sprite table
    for (s32 i = 0; i < 256; i++) {
        u8* ps = sprBase + (i * 8);
        u16 attrib = (static_cast<u16>(ps[6]) << 8) | ps[7]; // Attributes
        
        // Check for end of sprite list
        if (attrib >= 0xFF00) {
            spriteEnd = i;
            break;  // Found end marker, stop searching
        }
    }
    
    // Now render sprites in reverse order (last sprite on top)
    // We iterate backward from spriteEnd-1 down to 0
    for (s32 i = spriteEnd - 1; i >= 0; i--) {
        u8* ps = sprBase + (i * 8);
        
        // Read sprite data in correct order
        u16 xData = (static_cast<u16>(ps[0]) << 8) | ps[1];  // X position
        u16 yData = (static_cast<u16>(ps[2]) << 8) | ps[3];  // Y position
        u16 tileNum = (static_cast<u16>(ps[4]) << 8) | ps[5]; // Tile number
        u16 attrib = (static_cast<u16>(ps[6]) << 8) | ps[7]; // Attributes
        
        // Skip blank sprites
        if ((xData | attrib) == 0) continue;
        
        // Get sprite size (in 16x16 blocks)
        s32 bx = ((attrib >> 8) & 15) + 1;   // Width in tiles
        s32 by = ((attrib >> 12) & 15) + 1;  // Height in tiles
        
        // Map tile through graphics bank mapper
        s32 n = gfxRomBankMapper(cps::GFXTYPE_SPRITES, tileNum);
        if (n == -1) continue;
        
        // Add high bits from Y data
        n |= (yData & 0x6000) << 3;
        
        // Get X/Y coordinates (9-bit signed)
        s32 x = xData & 0x01FF;
        if (x >= 0x1C0) x -= 0x200;
        
        s32 y = yData & 0x01FF;
        y ^= 0x100;
        y -= 0x100;
        
        // Apply sprite offset
        x -= 0x40;
        y -= 0x10;
        
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
                
                // Calculate tile number for this part of sprite
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
// Tile Drawing Functions
// ============================================================================

inline void PPU::plotPixel(s32 x, s32 y, u32 color) {
    if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
        m_frameBuffer[y * SCREEN_WIDTH + x] = color;
    }
}

inline bool PPU::isPixelVisible(s32 x, s32 y) {
    return (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT);
}

void PPU::drawTile8x8(s32 x, s32 y, u32 tileAddr, u32 palette, u32 flip, bool clipCheck) {
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
                
                if (c && (!clipCheck || isPixelVisible(px, py))) {
                    plotPixel(px, py, pal[c]);
                }
            }
        } else {
            // Normal - read pixels from left to right
            // Pixel 0 from bits 28-31, pixel 1 from bits 24-27, etc.
            for (s32 tx = 0; tx < 8; tx++) {
                s32 px = x + tx;
                u8 c = (pix >> ((7 - tx) * 4)) & 0x0F;
                
                if (c && (!clipCheck || isPixelVisible(px, py))) {
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

void PPU::drawTile32x32(s32 x, s32 y, u32 tileAddr, u32 palette, u32 flip, bool clipCheck) {
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
                    
                    if (c && (!clipCheck || isPixelVisible(px, py))) {
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
                    
                    if (c && (!clipCheck || isPixelVisible(px, py))) {
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

} // namespace cps1
