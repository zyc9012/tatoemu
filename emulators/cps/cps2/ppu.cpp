#include "ppu.h"
#include "cartridge.h"
#include "../cpu.h"
#include <cstring>
#include <iostream>
#include <algorithm>

/*
 * CPS2 PPU Implementation
 * =======================
 * 
 * CPS2 PPU is very similar to CPS1 PPU with minor differences.
 * This is a simplified implementation that reuses CPS1 rendering logic.
 */

namespace cps2 {

// Separation table for graphics decoding (same as CPS1)
static u32 SepTable[256];
static bool SepTableInitialized = false;

static inline u32 Separate(u32 b) {
    u32 a = b;
    a = ((a & 0x000000F0) << 12) | (a & 0x0000000F);
    a = ((a & 0x000C000C) <<  6) | (a & 0x00030003);
    a = ((a & 0x02020202) <<  3) | (a & 0x01010101);
    return a;
}

static void InitSepTable() {
    if (SepTableInitialized) return;
    for (int i = 0; i < 256; i++) {
        SepTable[i] = Separate(255 - i);
    }
    SepTableInitialized = true;
}

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
}

void PPU::setCartridge(cps::CartridgeBase* cartridge) {
    m_cartridge = static_cast<Cartridge*>(cartridge);
}

void PPU::reset() {
    m_frameBuffer.fill(0);
    m_vram.fill(0);
    m_cpsRegs.fill(0);
    
    m_frameComplete = false;
    m_scanline = 0;
    m_cycles = 0;
    m_paletteNeedsUpdate = true;
    
    m_layer1XOffs = 0; m_layer1YOffs = 0;
    m_layer2XOffs = 0; m_layer2YOffs = 0;
    m_layer3XOffs = 0; m_layer3YOffs = 0;
    m_globalXOffs = 0; m_globalYOffs = 0;
    
    if (m_cartridge) {
        decodeGraphicsROM();
    }
}

void PPU::decodeGraphicsROM() {
    if (!m_cartridge) return;
    
    u32 srcSize = m_cartridge->getGraphicsROMSize();
    if (srcSize == 0) {
        std::cerr << "PPU: No graphics ROM data to decode" << std::endl;
        return;
    }
    
    // CPS2 graphics ROM decoding is similar to CPS1
    u32 dstSize = srcSize;
    m_decodedGfx.resize(dstSize, 0);
    m_gfxLen = dstSize;
    
    m_gfxMask = 1;
    while (m_gfxMask < m_gfxLen) {
        m_gfxMask <<= 1;
    }
    m_gfxMask -= 1;
    
    // Simplified decoding - same as CPS1
    u32 romChipSize = 0x80000;
    u32 numBanks = srcSize / (romChipSize * 4);
    if (numBanks == 0) numBanks = 1;
    
    for (u32 bank = 0; bank < numBanks; bank++) {
        u32 bankBase = bank * romChipSize * 4;
        u32 outBase = bank * romChipSize * 4;
        
        u32 rom0 = bankBase + 0 * romChipSize;
        u32 rom1 = bankBase + 1 * romChipSize;
        u32 rom2 = bankBase + 2 * romChipSize;
        u32 rom3 = bankBase + 3 * romChipSize;
        
        for (u32 i = 0; i < romChipSize && (rom0 + i + 1) < srcSize; i += 2) {
            u32 outOffset = outBase + (i / 2) * 8;
            
            if (rom0 + i + 1 < srcSize) {
                u8 b0 = m_cartridge->readGraphicsROM8(rom0 + i);
                u8 b1 = m_cartridge->readGraphicsROM8(rom0 + i + 1);
                u32 pix = SepTable[b0] | (SepTable[b1] << 1);
                if (outOffset < m_decodedGfx.size()) {
                    *reinterpret_cast<u32*>(m_decodedGfx.data() + outOffset) |= pix;
                }
            }
            
            if (rom1 + i + 1 < srcSize) {
                u8 b0 = m_cartridge->readGraphicsROM8(rom1 + i);
                u8 b1 = m_cartridge->readGraphicsROM8(rom1 + i + 1);
                u32 pix = SepTable[b0] | (SepTable[b1] << 1);
                pix <<= 2;
                if (outOffset < m_decodedGfx.size()) {
                    *reinterpret_cast<u32*>(m_decodedGfx.data() + outOffset) |= pix;
                }
            }
            
            if (rom2 + i + 1 < srcSize) {
                u8 b0 = m_cartridge->readGraphicsROM8(rom2 + i);
                u8 b1 = m_cartridge->readGraphicsROM8(rom2 + i + 1);
                u32 pix = SepTable[b0] | (SepTable[b1] << 1);
                if (outOffset + 4 < m_decodedGfx.size()) {
                    *reinterpret_cast<u32*>(m_decodedGfx.data() + outOffset + 4) |= pix;
                }
            }
            
            if (rom3 + i + 1 < srcSize) {
                u8 b0 = m_cartridge->readGraphicsROM8(rom3 + i);
                u8 b1 = m_cartridge->readGraphicsROM8(rom3 + i + 1);
                u32 pix = SepTable[b0] | (SepTable[b1] << 1);
                pix <<= 2;
                if (outOffset + 4 < m_decodedGfx.size()) {
                    *reinterpret_cast<u32*>(m_decodedGfx.data() + outOffset + 4) |= pix;
                }
            }
        }
    }
}

u8 PPU::readVRAM8(u32 address) {
    if (address < VRAM_SIZE) {
        return m_vram[address];
    }
    return 0;
}

u16 PPU::readVRAM16(u32 address) {
    if (address + 1 < VRAM_SIZE) {
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
        m_paletteNeedsUpdate = true;
    }
}

void PPU::writeVRAM16(u32 address, u16 value) {
    if (address + 1 < VRAM_SIZE) {
        m_vram[address] = (value >> 8) & 0xFF;
        m_vram[address + 1] = value & 0xFF;
        m_paletteNeedsUpdate = true;
    }
}

void PPU::writeVRAM32(u32 address, u32 value) {
    writeVRAM16(address, (value >> 16) & 0xFFFF);
    writeVRAM16(address + 2, value & 0xFFFF);
}

u8 PPU::readRegister8(u8 reg) {
    return m_cpsRegs[reg];
}

void PPU::writeRegister8(u8 reg, u8 value) {
    m_cpsRegs[reg] = value;
}

u8* PPU::findGfxRam(u32 address, u32 len) {
    address &= 0xFFFFFF;
    if (address >= 0x900000 && address + len <= 0x930000) {
        return m_vram.data() + (address - 0x900000);
    }
    return nullptr;
}

const u8* PPU::getGfxRom(u32 address) const {
    if (address < m_decodedGfx.size()) {
        return m_decodedGfx.data() + address;
    }
    return nullptr;
}

u32 PPU::convertPaletteEntry(u16 entry) {
    s32 bright = 0x0F + ((entry >> 12) << 1);
    s32 r = ((entry >> 8) & 0x0F) * 0x11 * bright / 0x2D;
    s32 g = ((entry >> 4) & 0x0F) * 0x11 * bright / 0x2D;
    s32 b = ((entry >> 0) & 0x0F) * 0x11 * bright / 0x2D;
    r = std::min(255, std::max(0, r));
    g = std::min(255, std::max(0, g));
    b = std::min(255, std::max(0, b));
    return 0xFF000000 | (r << 16) | (g << 8) | b;
}

void PPU::updatePalette() {
    u32 palAddr = (static_cast<u32>(m_cpsRegs[0x0A]) << 8) | m_cpsRegs[0x0B];
    palAddr <<= 8;
    palAddr &= 0xFFFC00;
    
    u32 palOffset = 0;
    if (palAddr >= 0x900000) {
        palOffset = palAddr - 0x900000;
    }
    
    if (palOffset >= VRAM_SIZE) {
        palOffset = 0;
    }
    
    for (u32 page = 0; page < 6; page++) {
        for (u32 i = 0; i < 0x200; i++) {
            u32 srcOffset = palOffset + (page * 0x400) + (i * 2);
            if (srcOffset + 1 < VRAM_SIZE) {
                u16 entry = (static_cast<u16>(m_vram[srcOffset]) << 8) | m_vram[srcOffset + 1];
                u32 dstIndex = (page * 0x200) + (i ^ 15);
                if (dstIndex < m_palette.size()) {
                    m_palette[dstIndex] = convertPaletteEntry(entry);
                }
            }
        }
    }
    
    m_paletteNeedsUpdate = false;
}

void PPU::step() {
    constexpr u32 CYCLES_PER_SCANLINE = 640;
    constexpr u32 VISIBLE_SCANLINES = 224;
    constexpr u32 TOTAL_SCANLINES = 262;
    constexpr u32 CYCLES_PER_FRAME = CYCLES_PER_SCANLINE * TOTAL_SCANLINES;
    
    m_cycles++;
    
    u32 newScanline = m_cycles / CYCLES_PER_SCANLINE;
    
    if (newScanline != m_scanline) {
        m_scanline = newScanline;
        
        if (m_scanline == VISIBLE_SCANLINES) {
            renderFrame();
            if (m_cpu) {
                m_cpu->irq(2);
            }
        }
    }
    
    if (m_cycles >= CYCLES_PER_FRAME) {
        m_cycles = 0;
        m_scanline = 0;
        m_frameComplete = true;
    }
}

void PPU::renderFrame() {
    if (!m_videoDevice) return;
    
    updatePalette();
    clearScreen();
    renderLayers();
    
    m_videoDevice->render(m_frameBuffer.data());
}

void PPU::clearScreen() {
    u32 bgColor = m_palette[0xBFF ^ 15];
    m_frameBuffer.fill(bgColor);
}

void PPU::renderLayers() {
    // Simplified layer rendering - same basic structure as CPS1
    // TODO: Implement full CPS2 layer rendering
    renderSprites();
}

void PPU::renderScroll1(const u8* base, s32 scrollX, s32 scrollY) {
    // TODO: Implement scroll layer 1
}

void PPU::renderScroll2(const u8* base, s32 scrollX, s32 scrollY) {
    // TODO: Implement scroll layer 2
}

void PPU::renderScroll3(const u8* base, s32 scrollX, s32 scrollY) {
    // TODO: Implement scroll layer 3
}

void PPU::renderSprites() {
    // Simplified sprite rendering - placeholder
    // TODO: Implement full CPS2 sprite rendering
}

void PPU::drawTile8x8(s32 x, s32 y, u32 tileAddr, u32 palette, u32 flip, bool clipCheck) {
    // TODO: Implement tile drawing
}

void PPU::drawTile16x16(s32 x, s32 y, u32 tileAddr, u32 palette, u32 flip, bool clipCheck) {
    // TODO: Implement tile drawing
}

void PPU::drawTile32x32(s32 x, s32 y, u32 tileAddr, u32 palette, u32 flip, bool clipCheck) {
    // TODO: Implement tile drawing
}

inline void PPU::plotPixel(s32 x, s32 y, u32 color) {
    if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
        m_frameBuffer[y * SCREEN_WIDTH + x] = color;
    }
}

inline bool PPU::isPixelVisible(s32 x, s32 y) {
    return (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT);
}

void PPU::saveState(std::ofstream& file) {
    file.write(reinterpret_cast<const char*>(m_vram.data()), m_vram.size());
    file.write(reinterpret_cast<const char*>(m_cpsRegs.data()), m_cpsRegs.size());
    file.write(reinterpret_cast<const char*>(&m_layer1XOffs), sizeof(m_layer1XOffs));
    file.write(reinterpret_cast<const char*>(&m_layer1YOffs), sizeof(m_layer1YOffs));
    file.write(reinterpret_cast<const char*>(&m_layer2XOffs), sizeof(m_layer2XOffs));
    file.write(reinterpret_cast<const char*>(&m_layer2YOffs), sizeof(m_layer2YOffs));
    file.write(reinterpret_cast<const char*>(&m_layer3XOffs), sizeof(m_layer3XOffs));
    file.write(reinterpret_cast<const char*>(&m_layer3YOffs), sizeof(m_layer3YOffs));
    file.write(reinterpret_cast<const char*>(&m_globalXOffs), sizeof(m_globalXOffs));
    file.write(reinterpret_cast<const char*>(&m_globalYOffs), sizeof(m_globalYOffs));
    file.write(reinterpret_cast<const char*>(m_gfxScroll), sizeof(m_gfxScroll));
    file.write(reinterpret_cast<const char*>(&m_frameComplete), sizeof(m_frameComplete));
    file.write(reinterpret_cast<const char*>(&m_scanline), sizeof(m_scanline));
    file.write(reinterpret_cast<const char*>(&m_cycles), sizeof(m_cycles));
    file.write(reinterpret_cast<const char*>(&m_paletteNeedsUpdate), sizeof(m_paletteNeedsUpdate));
}

void PPU::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(m_vram.data()), m_vram.size());
    file.read(reinterpret_cast<char*>(m_cpsRegs.data()), m_cpsRegs.size());
    file.read(reinterpret_cast<char*>(&m_layer1XOffs), sizeof(m_layer1XOffs));
    file.read(reinterpret_cast<char*>(&m_layer1YOffs), sizeof(m_layer1YOffs));
    file.read(reinterpret_cast<char*>(&m_layer2XOffs), sizeof(m_layer2XOffs));
    file.read(reinterpret_cast<char*>(&m_layer2YOffs), sizeof(m_layer2YOffs));
    file.read(reinterpret_cast<char*>(&m_layer3XOffs), sizeof(m_layer3XOffs));
    file.read(reinterpret_cast<char*>(&m_layer3YOffs), sizeof(m_layer3YOffs));
    file.read(reinterpret_cast<char*>(&m_globalXOffs), sizeof(m_globalXOffs));
    file.read(reinterpret_cast<char*>(&m_globalYOffs), sizeof(m_globalYOffs));
    file.read(reinterpret_cast<char*>(m_gfxScroll), sizeof(m_gfxScroll));
    file.read(reinterpret_cast<char*>(&m_frameComplete), sizeof(m_frameComplete));
    file.read(reinterpret_cast<char*>(&m_scanline), sizeof(m_scanline));
    file.read(reinterpret_cast<char*>(&m_cycles), sizeof(m_cycles));
    file.read(reinterpret_cast<char*>(&m_paletteNeedsUpdate), sizeof(m_paletteNeedsUpdate));
    m_paletteNeedsUpdate = true;
}

} // namespace cps2
