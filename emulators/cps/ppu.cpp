#include "ppu.h"
#include "cartridge.h"
#include "cpu.h"
#include "memory.h"
#include "consts.h"
#include <cstring>
#include <algorithm>

/*
 * PPU Implementation (CPS1 and CPS2)
 */

namespace cps {

// ============================================================================
// Constructor / Initialization
// ============================================================================

PPU::PPU()
    : m_cpu(nullptr)
    , m_cartridge(nullptr)
    , m_memory(nullptr)
    , m_videoDevice(nullptr)
    , m_isVertical(false)
    , m_gfxLen(0)
    , m_gfxMask(0)
    , m_layer1XOffs(0), m_layer1YOffs(0)
    , m_layer2XOffs(0), m_layer2YOffs(0)
    , m_layer3XOffs(0), m_layer3YOffs(0)
    , m_gfxMapper(nullptr)
    , m_scanline(0)
    , m_cycles(0)
    , m_cyclesPerFrame(0)
    , m_cyclesPerScanline(0)
    , m_rasterIrqCount(0)
    , m_paletteNeedsUpdate(true)
    , m_maxZValue(1)
    , m_maxZMask(0)
    , m_zOffset(0)
    , m_currentZValue(0)
    , m_bgHiMode(false)
{
    m_frameBuffer.fill(0);
    m_vram.fill(0);
    m_palette.fill(0);
    m_cpsRegs.fill(0);
    m_rasterLines.fill(0);
    
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
    m_rasterLines.fill(0);
    
    m_scanline = 0;
    m_cycles = 0;
    m_rasterIrqCount = 0;
    m_paletteNeedsUpdate = true;
    
    // Reset scroll offsets
    m_layer1XOffs = 0; m_layer1YOffs = 0;
    m_layer2XOffs = 0; m_layer2YOffs = 0;
    m_layer3XOffs = 0; m_layer3YOffs = 0;
    
    // Reset CPS2 Z-buffer state
    m_maxZValue = 1;
    m_maxZMask = 0;
    m_zOffset = 0;
    m_currentZValue = 0;
    
    // Reset masking and blending
    m_bgHiMode = false;
    
    // Reset raster zones (default: single zone covering whole screen)
    m_rasterLines[0] = 0;        // Zone 0 starts at scanline 0
    m_rasterLines[1] = 0;        // No additional zones by default
    for (s32 i = 2; i < MAX_RASTER + 2; i++) {
        m_rasterLines[i] = 0;
    }

    if (m_cartridge->getCPSVersion() == 2) {
        m_cyclesPerFrame = ::cps2::CPU_CYCLES_PER_FRAME;
    } else if (m_cartridge->isCPS1QSound()) {
        m_cyclesPerFrame = ::cps1qs::CPU_CYCLES_PER_FRAME;
    } else {
        m_cyclesPerFrame = ::cps1::CPU_CYCLES_PER_FRAME;
    }

    m_cyclesPerScanline = m_cyclesPerFrame / TOTAL_SCANLINES;
    
    if (m_cartridge) {
        u8 cpsVer = m_cartridge->getCPSVersion();
        m_boardConfig = m_cartridge->getBoardConfig();

        const GameInfo* gameInfo = m_cartridge->getGameInfo();
        if (gameInfo) {
            m_isVertical = (gameInfo->flags & GameFlags::GAME_FLAG_VERTICAL_SCREEN) != 0;

            m_is_xmcota = (strcmp(gameInfo->romSetName, "xmcota") == 0);
            bool is_hsf2 = (strcmp(gameInfo->romSetName, "hsf2") == 0);
            m_is_ssf2 = (strncmp(gameInfo->romSetName, "ssf2", 4) == 0) || is_hsf2;
            m_is_ssf2t = (strcmp(gameInfo->romSetName, "ssf2t") == 0) || is_hsf2;
        }
        
        if (cpsVer == 1) {
            setupGraphicsMapper();
            m_gfxScroll[1] = 0;
            m_gfxScroll[2] = 0;
            m_gfxScroll[3] = 0;
        } else {
            m_zBuffer.resize(SCREEN_WIDTH * SCREEN_HEIGHT, 0);
            m_gfxScroll[1] = 0x800000;
            m_gfxScroll[2] = 0x800000;
            m_gfxScroll[3] = 0x800000;
            if (m_is_ssf2) {
                m_gfxScroll[3] = 0;
            }
        }
    }
}

void PPU::setupGraphicsMapper() {
    if (!m_cartridge) return;
    
    CPSMapper mapper = m_cartridge->getMapper();
    GameDatabase::getGraphicsMapper(mapper, m_gfxBankSizes, m_gfxMapper);
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

u16 PPU::readRegister16(u8 reg) {
    return (static_cast<u16>(m_cpsRegs[reg]) << 8) | m_cpsRegs[reg + 1];
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

void PPU::setDecodedGraphics(const std::vector<u8>& decodedGfx) {
    m_decodedGfx = decodedGfx;
    m_gfxLen = static_cast<u32>(m_decodedGfx.size());
    
    // Calculate mask for graphics address
    m_gfxMask = 1;
    while (m_gfxMask < m_gfxLen) {
        m_gfxMask <<= 1;
    }
    m_gfxMask -= 1;
}

const u8* PPU::getGfxRom(u32 address) const {
    if (address < m_gfxLen && !m_decodedGfx.empty()) {
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
        m_maxZMask = 0;
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
        log_error("Palette Update: palOffset out of bounds: 0x%x", palOffset);
    }
    
    // Get palette control register (which pages to update)
    u8 palCtrlReg = m_boardConfig.paletteControlReg;
    u8 palCtrl = m_cpsRegs[palCtrlReg ^ 1];
    
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

void PPU::step(u32 cycles) {
    // CPS1/CPS2 render a full frame at VBlank
    // Frame rate is ~59.63Hz for both systems
    // CPS1: 68000 runs at 10MHz, CPS2: 68000 runs at 16MHz
    // CPU cycles per frame ~= CPU_FREQUENCY / 59.63
    
    // Increment cycle counter
    m_cycles += cycles;
    
    // Calculate current scanline
    u32 newScanline = m_cycles / m_cyclesPerScanline;
    
    // Check if we've moved to a new scanline
    if (newScanline != m_scanline) {
        m_scanline = newScanline;
        
        // CPS2: Handle raster interrupts
        if (m_cartridge && m_cartridge->getCPSVersion() == 2 && m_cpu) {
            processCPS2RasterInterrupts();
        }
        
        // At VBlank start (scanline 224 + 16 = 240), render the frame and trigger interrupt
        static constexpr u32 nVBlankLine = FIRST_VISIBLE_SCANLINE + VISIBLE_SCANLINES;
        if (m_scanline == nVBlankLine) {
            renderFrame();

            // Trigger VBlank interrupt (priority 2)
            if (m_cpu) {
                m_cpu->irq(2);
            }
        }
    }
    
    // Check if frame is complete
    if (m_cycles >= m_cyclesPerFrame) {
        m_cycles -= m_cyclesPerFrame;
        m_scanline = 0;
        m_rasterIrqCount = 0;  // Reset raster IRQ count for next frame
        
        // Reset raster lines for next frame
        for (s32 i = 0; i < MAX_RASTER + 2; i++) {
            m_rasterLines[i] = 0;
        }
    }
}

void PPU::processCPS2RasterInterrupts() {
    // This function is called at the start of each scanline.
    // At first visible line, copy initial register state to zone 0
    if (m_scanline == static_cast<u32>(FIRST_VISIBLE_SCANLINE)) {
        copyRegistersToZone(0);
        copyFrgRegistersToZone(0);
    }
    
    // Read IRQ control register (0x4E-0x4F)
    u16 irqCtrl = (static_cast<u16>(m_cpsRegs[0x4E]) << 8) | m_cpsRegs[0x4F];
    bool beamSyncEnabled = (irqCtrl & 0x0200) == 0;
    
    auto processIrq = [this, beamSyncEnabled](u8 irqLineReg) {
        u16 irqReg = (static_cast<u16>(m_cpsRegs[irqLineReg]) << 8) | m_cpsRegs[irqLineReg + 1];
        bool autoIrq = (irqReg & 0x8000) != 0;
        u32 irqLine = irqReg & 0x01FF;

        if ((autoIrq || beamSyncEnabled) && irqLine < TOTAL_SCANLINES) {
            // Check if we are on the scanline for this interrupt
            if (m_scanline == irqLine) {
                // Trigger IRQ (level 4 for raster interrupts)
                m_cpu->irq(4);
    
                // Handle auto-IRQ mode
                if (autoIrq) {
                    irqLine += 32;
                    m_cpsRegs[irqLineReg] = ((irqLine >> 8) & 0x01) | 0x80;  // Preserve auto-IRQ flag
                    m_cpsRegs[irqLineReg + 1] = irqLine & 0xFF;
                }
            }
    
            // Let CPU process the irq for one more scanline and then save registers
            if (m_scanline == irqLine + 1) {
                // Record raster line for this interrupt zone
                if (irqLine >= FIRST_VISIBLE_SCANLINE) {
                    m_rasterIrqCount++;
                    s32 rasterLine = irqLine - FIRST_VISIBLE_SCANLINE;
                    if (rasterLine < static_cast<s32>(VISIBLE_SCANLINES)) {
                        setRasterLine(m_rasterIrqCount, rasterLine);
                        copyRegistersToZone(m_rasterIrqCount);
                        copyFrgRegistersToZone(m_rasterIrqCount);
                    } else {
                        setRasterLine(m_rasterIrqCount, 0);
                    }
                }
            }
        }
    };

    // Process IRQ line 50
    processIrq(0x50);

    // Process IRQ line 52
    processIrq(0x52);
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
    bool layer1Enable = (layerCtrl & m_boardConfig.layerEnable[1]) != 0;
    bool layer2Enable = (layerCtrl & m_boardConfig.layerEnable[2]) != 0;
    bool layer3Enable = (layerCtrl & m_boardConfig.layerEnable[3]) != 0;
    
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
    scr1X += 0x40 + m_layer1XOffs;
    scr1Y += 0x10 + m_layer1YOffs;
    
    scr2X += 0x40 + m_layer2XOffs;
    scr2Y += 0x10 + m_layer2YOffs;
    
    scr3X += 0x40 + m_layer3XOffs;
    scr3Y += 0x10 + m_layer3YOffs;
    
    // Find VRAM pointers
    u8* scr1Base = findGfxRam(scr1Off, 0x4000);
    u8* scr2Base = findGfxRam(scr2Off, 0x4000);
    u8* scr3Base = findGfxRam(scr3Off, 0x4000);
    
    // Render layers from bottom to top
    for (int i = 3; i >= 0; i--) {
        s32 n = draw[i];
        
        switch (n) {
            case 0:  // Sprites
                if (drawMask & 1) {
                    renderSpritesCPS1();
                }

                if (i + 1 < 4) {
                    m_bgHiMode = true;
                    switch (draw[i + 1]) {
                        case 1:
                            if ((drawMask & 2) && scr1Base) {
                                renderScroll1(scr1Base, scr1X, scr1Y, 0, SCREEN_HEIGHT);
                            }
                            break;
                        case 2:
                            if ((drawMask & 4) && scr2Base) {
                                renderScroll2(scr2Base, scr2X, scr2Y, 0, SCREEN_HEIGHT);
                            }
                            break;
                        case 3:
                            if ((drawMask & 8) && scr3Base) {
                                renderScroll3(scr3Base, scr3X, scr3Y, 0, SCREEN_HEIGHT);
                            }
                            break;
                    }
                    m_bgHiMode = false;
                }
                break;
                
            case 1:  // Scroll 1 (8x8 tiles)
                if ((drawMask & 2) && scr1Base) {
                    renderScroll1(scr1Base, scr1X, scr1Y, 0, SCREEN_HEIGHT);
                }
                break;
                
            case 2:  // Scroll 2 (16x16 tiles)
                if ((drawMask & 4) && scr2Base) {
                    renderScroll2(scr2Base, scr2X, scr2Y, 0, SCREEN_HEIGHT);
                }
                break;
                
            case 3:  // Scroll 3 (32x32 tiles)
                if ((drawMask & 8) && scr3Base) {
                    renderScroll3(scr3Base, scr3X, scr3Y, 0, SCREEN_HEIGHT);
                }
                break;
        }
    }
}

void PPU::renderLayersCPS2() {
    // CPS2 raster-based priority rendering
    
    // Initialize Z-buffer for CPS2 sprite rendering
    initCPS2ZBuffer();
    
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
        
        // Use board configuration for layer control register
        u8 lcReg = m_boardConfig.layerControlReg;
        u16 layerCtrl = (static_cast<u16>(regs[lcReg]) << 8) | regs[lcReg + 1];
        
        // Determine which layers are enabled using board-specific enable bits
        bool layer1Enable = (layerCtrl & m_boardConfig.layerEnable[1]) != 0;
        bool layer2Enable = (layerCtrl & m_boardConfig.layerEnable[2]) != 0;
        bool layer3Enable = (layerCtrl & m_boardConfig.layerEnable[3]) != 0;
        
        // Determine layer order (3=top, 0=bottom)
        draw[numZones][3] = (layerCtrl >> 12) & 3;  // Top layer
        draw[numZones][2] = (layerCtrl >> 10) & 3;
        draw[numZones][1] = (layerCtrl >> 8) & 3;
        draw[numZones][0] = (layerCtrl >> 6) & 3;   // Bottom layer
        
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
                        renderSpritesCPS2(prevPrio + 1, currPrio);
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
                    scr1X += 0x40 + m_layer1XOffs;
                    scr1Y += 0x10 + m_layer1YOffs;
                    
                    scr2X += 0x40 + m_layer2XOffs;
                    scr2Y += 0x10 + m_layer2YOffs;
                    
                    scr3X += 0x40 + m_layer3XOffs;
                    scr3Y += 0x10 + m_layer3YOffs;
                    
                    // Render the appropriate layer
                    switch (layerNum) {
                        case 1:  // Scroll 1 (8x8 tiles)
                            if (drawMask[nSlice] & 2) {
                                u8* scr1Base = findGfxRam(scr1Off, 0x4000);
                                if (scr1Base) {
                                    renderScroll1(scr1Base, scr1X, scr1Y, startLine, endLine);
                                }
                            }
                            break;
                            
                        case 2:  // Scroll 2 (16x16 tiles)
                            if (drawMask[nSlice] & 4) {
                                u8* scr2Base = findGfxRam(scr2Off, 0x4000);
                                if (scr2Base) {
                                    renderScroll2(scr2Base, scr2X, scr2Y, startLine, endLine);
                                }
                            }
                            break;
                            
                        case 3:  // Scroll 3 (32x32 tiles)
                            if (drawMask[nSlice] & 8) {
                                u8* scr3Base = findGfxRam(scr3Off, 0x4000);
                                if (scr3Base) {
                                    renderScroll3(scr3Base, scr3X, scr3Y, startLine, endLine);
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
        renderSpritesCPS2(prevPrio + 1, 7);
    }
}

// ============================================================================
// Scroll 1 (8x8 tiles)
// ============================================================================

void PPU::renderScroll1(const u8* base, s32 scrollX, s32 scrollY, s32 startLine, s32 endLine) {
    if (!base || m_decodedGfx.empty()) return;
    
    s32 ix = (scrollX >> 3) + 1;
    s32 iy = (scrollY >> 3) + 1;
    s32 sx = 8 - (scrollX & 7);
    s32 sy = 8 - (scrollY & 7);
    
    s32 nXTile = SCREEN_WIDTH >> 3;   // 48 tiles
    s32 nYTile = SCREEN_HEIGHT >> 3;  // 28 tiles

    u8 cpsVer = m_cartridge->getCPSVersion();

    // Determine Y range: CPS1 always full screen, CPS2 uses partial scanline rendering
    s32 yStart, yEnd;
    if (cpsVer == 1) {
        // CPS1: full screen rendering
        yStart = -1;
        yEnd = nYTile;
    } else {
        // CPS2: partial scanline rendering
        s32 nFirstY = (startLine + (scrollY & 7)) >> 3;
        s32 nLastY = (endLine + (scrollY & 7)) >> 3;
        yStart = nFirstY - 1;
        yEnd = nLastY;
    }

    for (s32 y = yStart; y < yEnd; y++) {
        // Check if this row intersects with our scanline range (CPS2 only)
        bool clipY = (cpsVer == 2) &&
                    (((y << 3) < startLine) || (((y << 3) + 8) >= endLine));

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
            bool clipCheck = (x < 0 || x >= nXTile - 1 || y < 0 || y >= nYTile - 1 || clipY);

            if (m_bgHiMode) {
                // Read mask from CPS register at offset specified by maskAddr
                u8 maskReg = m_boardConfig.maskAddr[(attrib & 0x180) >> 7];
                u16 mask = (static_cast<u16>(m_cpsRegs[maskReg]) << 8) | m_cpsRegs[maskReg + 1];
                if (mask != 0) {
                    drawTile8x8(px, py, tileAddr, palette, flip, clipCheck, mask);
                }
            } else {
                drawTile8x8(px, py, tileAddr, palette, flip, clipCheck, 0);
            }
        }
    }
}

// ============================================================================
// Scroll 2 (16x16 tiles with optional row scroll)
// ============================================================================

void PPU::renderScroll2(const u8* base, s32 scrollX, s32 scrollY, s32 startLine, s32 endLine) {
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

    u8 cpsVer = m_cartridge->getCPSVersion();

    // Determine Y range: CPS1 always full screen, CPS2 uses partial scanline rendering
    s32 yStart, yEnd;
    if (cpsVer == 1) {
        // CPS1: full screen rendering
        yStart = -1;
        yEnd = nYTile;
    } else {
        // CPS2: partial scanline rendering
        s32 nFirstY = (startLine + (scrollY & 15)) >> 4;
        s32 nLastY = (endLine + (scrollY & 15)) >> 4;
        yStart = nFirstY - 1;
        yEnd = nLastY;
    }

    for (s32 y = yStart; y < yEnd; y++) {
        // Check if this row intersects with our scanline range (CPS2 only)
        bool clipY = (cpsVer == 2) &&
                    (((y << 4) < startLine) || (((y << 4) + 16) >= endLine));
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
            bool clipCheck = (x < 0 || x >= nXTile - 1 || y < 0 || y >= nYTile - 1 || clipY);
            
            if (m_bgHiMode) {
                // Read mask from CPS register at offset specified by maskAddr
                u8 maskReg = m_boardConfig.maskAddr[(attrib & 0x180) >> 7];
                u16 mask = (static_cast<u16>(m_cpsRegs[maskReg]) << 8) | m_cpsRegs[maskReg + 1];
                if (mask != 0) {
                    drawTile16x16(px, py, tileAddr, palette, flip, clipCheck, mask);
                }
            } else {
                drawTile16x16(px, py, tileAddr, palette, flip, clipCheck, 0);
            }
            
        }
    }
}


// ============================================================================
// Scroll 3 (32x32 tiles)
// ============================================================================

void PPU::renderScroll3(const u8* base, s32 scrollX, s32 scrollY, s32 startLine, s32 endLine) {
    if (!base || m_decodedGfx.empty()) return;
    
    s32 ix = (scrollX >> 5) + 1;
    s32 iy = (scrollY >> 5) + 1;
    s32 sx = 32 - (scrollX & 31);
    s32 sy = 32 - (scrollY & 31);
    
    s32 nXTile = SCREEN_WIDTH >> 5;   // 12 tiles
    s32 nYTile = SCREEN_HEIGHT >> 5;  // 7 tiles
    
    u8 cpsVer = m_cartridge->getCPSVersion();

    // Determine Y range: CPS1 always full screen, CPS2 uses partial scanline rendering
    s32 yStart, yEnd;
    if (cpsVer == 1) {
        // CPS1: full screen rendering
        yStart = -1;
        yEnd = nYTile;
    } else {
        // CPS2: partial scanline rendering
        s32 nFirstY = (startLine + (scrollY & 31)) >> 5;
        s32 nLastY = (endLine + (scrollY & 31)) >> 5;
        yStart = nFirstY - 1;
        yEnd = nLastY;
    }

    for (s32 y = yStart; y < yEnd; y++) {
        // Check if this row intersects with our scanline range (CPS2 only)
        bool clipY = (cpsVer == 2) &&
                    (((y << 5) < startLine) || (((y << 5) + 32) >= endLine));

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
            
            // CPS2 special tile offset hacks for some games
            if (m_is_xmcota && tileNum >= 0x5800) tileNum -= 0x4000;
            if (m_is_ssf2t && tileNum < 0x5600) tileNum += 0x4000;

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
            bool clipCheck = (x < 0 || x >= nXTile - 1 || y < 0 || y >= nYTile - 1 || clipY);
            
            if (m_bgHiMode) {
                // Read mask from CPS register at offset specified by maskAddr
                u8 maskReg = m_boardConfig.maskAddr[(attrib & 0x180) >> 7];
                u16 mask = (static_cast<u16>(m_cpsRegs[maskReg]) << 8) | m_cpsRegs[maskReg + 1];
                if (mask != 0) {
                    drawTile32x32(px, py, tileAddr, palette, flip, clipCheck, mask);
                }
            } else {
                drawTile32x32(px, py, tileAddr, palette, flip, clipCheck, 0);
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

void PPU::renderSpritesCPS2(s32 levelFrom, s32 levelTo) {
    if (m_decodedGfx.empty()) return;
    
    // CPS2 supports up to 1024 sprites (8 bytes each = 8KB max)
    s32 maxSprites = 1024;
    
    // Find where the sprite list ends
    s32 spriteEnd = maxSprites;
    for (s32 i = 0; i < maxSprites; i++) {
        u32 sprAddr = i * 8;
        if (sprAddr + 7 >= 0x8000) break;  // Beyond 32KB visible object RAM
        
        // Read sprite words (big-endian format) for end detection
        u16 yData = readObjRAM16(sprAddr + 2);
        u16 attrib = readObjRAM16(sprAddr + 6);
        
        // CPS2 end of sprite list: word1 & 0x8000 or attrib >= 0xFF00
        if ((yData & 0x8000) || (attrib >= 0xFF00)) {
            spriteEnd = i;
            break;
        }
    }
    
    // Get sprite offsets from CPS2 Frg registers (use saved Frg for zone 0)
    s16 sprXOffset = -static_cast<s16>(m_rasterFrg[0][0x09]);
    s16 sprYOffset = -static_cast<s16>(m_rasterFrg[0][0x0B]);
    
    // Iterate through sprites
    // Sprites are processed in order (not reversed like CPS1)
    m_currentZValue = static_cast<u16>(m_maxZValue);
    bool higherPriorityFound = false;  // Track if we've encountered sprites with higher priority
    
    for (s32 i = 0; i < spriteEnd; i++) {
        u32 sprAddr = i * 8;
        
        // Read sprite data from object RAM (big-endian format)
        u16 xData = readObjRAM16(sprAddr + 0);
        u16 yData = readObjRAM16(sprAddr + 2);
        u16 tileNum = readObjRAM16(sprAddr + 4);
        u16 attrib = readObjRAM16(sprAddr + 6);
        
        // Get sprite priority from bits [15:13] of xData
        u32 priority = (xData >> 13) & 7;
        
        // Check if sprite priority is above our rendering range
        if (static_cast<s32>(priority) > levelTo) {
            higherPriorityFound = true;  // Mark that higher priority sprites exist
            m_currentZValue++;
            continue;
        }
        
        // Check if sprite priority is below our rendering range
        if (static_cast<s32>(priority) < levelFrom) {
            m_currentZValue++;
            continue;
        }
        
        // Skip blank sprites
        if ((xData | attrib) == 0) {
            m_currentZValue++;
            continue;
        }
        
        // Update Z-buffer tracking based on whether higher priority sprites exist
        if (higherPriorityFound) {
            m_maxZMask = m_currentZValue;
        } else {
            m_maxZValue = m_currentZValue;
        }
        
        // Determine if we should use Z-buffer masking for this sprite
        // Use Z-buffer if: we found higher priority sprites OR mask value exceeds normal value
        bool useZBuffer = higherPriorityFound || (m_maxZMask > m_maxZValue);
        
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
                
                // Draw with Z-buffer support if needed
                drawTile16x16(px, py, tileAddr, palette, flip, clipCheck, 0, useZBuffer);
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
        if (m_isVertical) {
            m_frameBuffer[(SCREEN_WIDTH - x - 1) * SCREEN_HEIGHT + y] = color;
        } else {
            m_frameBuffer[y * SCREEN_WIDTH + x] = color;
        }
    }
}

inline void PPU::plotPixelWithZ(s32 x, s32 y, u32 color) {
    if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
        u32 offset;
        if (m_isVertical) {
            offset = (SCREEN_WIDTH - x - 1) * SCREEN_HEIGHT + y;
        } else {
            offset = y * SCREEN_WIDTH + x;
        }
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
                
                // Draw: if no mask draw all, if mask only draw where bit is SET
                if (c && (!clipCheck || isPixelVisible(px, py))) {
                    if (mask == 0 || (mask & (1 << (c ^ 15)))) {
                        plotPixel(px, py, pal[c]);
                    }
                }
            }
        } else {
            // Normal - read pixels from left to right
            // Pixel 0 from bits 28-31, pixel 1 from bits 24-27, etc.
            for (s32 tx = 0; tx < 8; tx++) {
                s32 px = x + tx;
                u8 c = (pix >> ((7 - tx) * 4)) & 0x0F;
                
                // Draw: if no mask draw all, if mask only draw where bit is SET
                if (c && (!clipCheck || isPixelVisible(px, py))) {
                    if (mask == 0 || (mask & (1 << (c ^ 15)))) {
                        plotPixel(px, py, pal[c]);
                    }
                }
            }
        }
    }
}

void PPU::drawTile16x16(s32 x, s32 y, u32 tileAddr, u32 palette, u32 flip, bool clipCheck, u16 mask, bool useZ) {
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

    auto plotFunc = [useZ, this](s32 x, s32 y, u32 color) {
        if (useZ) {
            plotPixelWithZ(x, y, color);
        } else {
            plotPixel(x, y, color);
        }
    };

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
                
                // Draw: if no mask draw all, if mask only draw where bit is SET
                if (c && (!clipCheck || isPixelVisible(px, py))) {
                    if (mask == 0 || (mask & (1 << (c ^ 15)))) {
                        plotFunc(px, py, pal[c]);
                    }
                }
            }
            // Draw first word's pixels reversed
            for (s32 tx = 0; tx < 8; tx++) {
                s32 px = x + 8 + tx;
                u8 c = (pix0 >> (tx * 4)) & 0x0F;

                // Draw: if no mask draw all, if mask only draw where bit is SET
                if (c && (!clipCheck || isPixelVisible(px, py))) {
                    if (mask == 0 || (mask & (1 << (c ^ 15)))) {
                        plotFunc(px, py, pal[c]);
                    }
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
                
                // Draw: if no mask draw all, if mask only draw where bit is SET
                if (c && (!clipCheck || isPixelVisible(px, py))) {
                    if (mask == 0 || (mask & (1 << (c ^ 15)))) {
                        plotFunc(px, py, pal[c]);
                    }
                }
            }
            // Draw second word (pixels 8-15)
            for (s32 tx = 0; tx < 8; tx++) {
                s32 px = x + 8 + tx;
                u8 c = (pix1 >> ((7 - tx) * 4)) & 0x0F;
                
                // Draw: if no mask draw all, if mask only draw where bit is SET
                if (c && (!clipCheck || isPixelVisible(px, py))) {
                    if (mask == 0 || (mask & (1 << (c ^ 15)))) {
                        plotFunc(px, py, pal[c]);
                    }
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
                    
                    // Draw: if no mask draw all, if mask only draw where bit is SET
                    if (c && (!clipCheck || isPixelVisible(px, py))) {
                        if (mask == 0 || (mask & (1 << (c ^ 15)))) {
                            plotPixel(px, py, pal[c]);
                        }
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
                    
                    // Draw: if no mask draw all, if mask only draw where bit is SET
                    if (c && (!clipCheck || isPixelVisible(px, py))) {
                        if (mask == 0 || (mask & (1 << (c ^ 15)))) {
                            plotPixel(px, py, pal[c]);
                        }
                    }
                }
            }
        }
    }
}

// ============================================================================
// Save/Load State
// ============================================================================

void PPU::saveState(Buffer* buf) {
    // Save VRAM
    buffer_write(buf, m_vram.data(), m_vram.size());
    
    // Save registers
    buffer_write(buf, m_cpsRegs.data(), m_cpsRegs.size());
    
    // Save scroll offsets
    buffer_write(buf, &m_layer1XOffs, sizeof(m_layer1XOffs));
    buffer_write(buf, &m_layer1YOffs, sizeof(m_layer1YOffs));
    buffer_write(buf, &m_layer2XOffs, sizeof(m_layer2XOffs));
    buffer_write(buf, &m_layer2YOffs, sizeof(m_layer2YOffs));
    buffer_write(buf, &m_layer3XOffs, sizeof(m_layer3XOffs));
    buffer_write(buf, &m_layer3YOffs, sizeof(m_layer3YOffs));
    
    // Save graphics scroll offsets
    buffer_write(buf, m_gfxScroll, sizeof(m_gfxScroll));
    
    // Save state flags
    buffer_write(buf, &m_scanline, sizeof(m_scanline));
    buffer_write(buf, &m_cycles, sizeof(m_cycles));
    buffer_write(buf, &m_cyclesPerFrame, sizeof(m_cyclesPerFrame));
    buffer_write(buf, &m_cyclesPerScanline, sizeof(m_cyclesPerScanline));
    buffer_write(buf, &m_paletteNeedsUpdate, sizeof(m_paletteNeedsUpdate));
}

void PPU::loadState(Buffer* buf) {
    // Load VRAM
    buffer_read(buf, m_vram.data(), m_vram.size());
    
    // Load registers
    buffer_read(buf, m_cpsRegs.data(), m_cpsRegs.size());
    
    // Load scroll offsets
    buffer_read(buf, &m_layer1XOffs, sizeof(m_layer1XOffs));
    buffer_read(buf, &m_layer1YOffs, sizeof(m_layer1YOffs));
    buffer_read(buf, &m_layer2XOffs, sizeof(m_layer2XOffs));
    buffer_read(buf, &m_layer2YOffs, sizeof(m_layer2YOffs));
    buffer_read(buf, &m_layer3XOffs, sizeof(m_layer3XOffs));
    buffer_read(buf, &m_layer3YOffs, sizeof(m_layer3YOffs));
    
    // Load graphics scroll offsets
    buffer_read(buf, m_gfxScroll, sizeof(m_gfxScroll));
    
    // Load state flags
    buffer_read(buf, &m_scanline, sizeof(m_scanline));
    buffer_read(buf, &m_cycles, sizeof(m_cycles));
    buffer_read(buf, &m_cyclesPerFrame, sizeof(m_cyclesPerFrame));
    buffer_read(buf, &m_cyclesPerScanline, sizeof(m_cyclesPerScanline));
    buffer_read(buf, &m_paletteNeedsUpdate, sizeof(m_paletteNeedsUpdate));
    
    // Force palette update after loading state
    m_paletteNeedsUpdate = true;
}

// ============================================================================
// CPS2 Memory Access Helpers
// ============================================================================

u8 PPU::readObjRAM8(u32 offset) {
    if (!m_memory) return 0;
    // Object RAM is at 0x708000-0x70FFFF (32KB visible at a time due to banking)
    if (offset >= 0x8000) return 0;  // 32KB max visible
    return m_memory->read8(0x708000 + offset);
}

u16 PPU::readObjRAM16(u32 offset) {
    if (!m_memory) return 0;
    if (offset >= 0x8000) return 0;
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
    // Set the scanline boundary for a raster zone.
    if (zone >= MAX_RASTER + 2) return;
    m_rasterLines[zone] = scanline;
}

void PPU::copyRegistersToZone(u32 zone) {
    if (zone >= MAX_RASTER) return;
    
    // Copy all 256 CPS registers to this zone's register set
    std::copy(m_cpsRegs.begin(), m_cpsRegs.end(), m_rasterRegs[zone].begin());
}

void PPU::copyFrgRegistersToZone(u32 zone) {
    if (zone >= MAX_RASTER) return;
    if (!m_memory) return;
    
    // Copy 16 bytes of Frg registers
    for (u8 i = 0; i < 16; i++) {
        m_rasterFrg[zone][i] = readFrgReg8(i);
    }
}

} // namespace cps
