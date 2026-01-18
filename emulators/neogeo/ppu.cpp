#include "ppu.h"
#include "cpu.h"
#include "cartridge.h"
#include "memory.h"
#include <cstring>
#include <algorithm>

namespace neogeo {

PPU::PPU()
    : m_cpu(nullptr)
    , m_cartridge(nullptr)
    , m_memory(nullptr)
    , m_videoDevice(nullptr)
    , m_vramPointer(0)
    , m_vramModulo(0)
    , m_vramBank(0)
    , m_frameComplete(false)
    , m_scanline(0)
    , m_cycles(0)
    , m_spriteFrame(0)
    , m_spriteFrameTimer(0) {
    m_frameBuffer.fill(0);
    m_vram.fill(0);
}

void PPU::reset() {
    m_frameBuffer.fill(0);
    m_vram.fill(0);
    m_vramPointer = 0;
    m_vramModulo = 0;
    m_vramBank = 0;
    m_frameComplete = false;
    m_scanline = 0;
    m_cycles = 0;
    m_spriteFrame = 0;
    m_spriteFrameTimer = 0;
}

void PPU::step() {
    m_cycles++;
    
    // NeoGeo timing: ~12MHz CPU, ~59.185 Hz refresh, 264 scanlines
    u32 cyclesPerFrame = CPU_FREQUENCY / static_cast<u32>(TARGET_FPS);
    u32 cyclesPerScanline = cyclesPerFrame / TOTAL_SCANLINES;
    
    u32 newScanline = (m_cycles / cyclesPerScanline) % TOTAL_SCANLINES;
    
    if (newScanline != m_scanline) {
        u32 prevScanline = m_scanline;
        m_scanline = newScanline;
        
        // VBlank starts at scanline 248 - fire VBlank IRQ (level 1 for cart systems)
        // This is critical for games to run - they wait for VBlank interrupt
        if (prevScanline < 248 && m_scanline >= 248) {
            if (m_cpu) {
                m_cpu->irq(1);  // VBlank IRQ is level 1 for cartridge systems
            }
        }
        
        // Frame complete at end of frame (scanline wraps to 0)
        if (m_scanline == 0) {
            // Render the entire frame
            renderFrame();
            
            m_frameComplete = true;
            m_cycles = 0;
            
            // Update sprite animation frame
            m_spriteFrameTimer++;
            // Auto-animation timing (simplified)
            
            // Present frame to video device
            if (m_videoDevice) {
                m_videoDevice->render(m_frameBuffer.data());
            }
        }
    }
}

u16 PPU::readVRAM() const {
    u32 addr = m_vramPointer;
    if (m_vramBank) {
        addr += 0x10000;
    }
    return readVRAM16(addr);
}

void PPU::writeVRAM(u16 value) {
    u32 addr = m_vramPointer;
    if (m_vramBank) {
        addr += 0x10000;
    }
    writeVRAM16(addr, value);
    m_vramPointer = (m_vramPointer + m_vramModulo) & 0xFFFF;
}

void PPU::setVRAMPointer(u16 value) {
    m_vramPointer = (value & 0x7FFF) << 1;
    m_vramBank = (value & 0x8000) ? 1 : 0;
}

void PPU::setVRAMModulo(s16 value) {
    m_vramModulo = static_cast<s32>(value) << 1;
}

u16 PPU::readVRAM16(u32 address) const {
    address &= 0x1FFFF;  // 128KB
    return static_cast<u16>(m_vram[address]) | 
           (static_cast<u16>(m_vram[address + 1]) << 8);
}

void PPU::writeVRAM16(u32 address, u16 value) {
    address &= 0x1FFFF;  // 128KB
    m_vram[address] = static_cast<u8>(value & 0xFF);
    m_vram[address + 1] = static_cast<u8>(value >> 8);
}

void PPU::renderFrame() {
    // Clear frame buffer to background color (palette 0, color 0)
    u32 bgColor = getPaletteColor(0, 0);
    m_frameBuffer.fill(bgColor);
    
    // Render sprites (back to front, sprites have priority)
    renderSprites();
    
    // Render text layer (fix layer, always on top)
    renderText();
}

void PPU::renderSprites() {
    if (!m_cartridge) return;
    
    // NeoGeo has 381 sprite "banks" (0-380)
    // Each sprite can be multiple tiles tall (up to 32 tiles, 512 pixels)
    
    // Sprite Control Blocks in VRAM:
    // SCB1 (0x0000-0x6FFF): Tile number + palette for each row
    // SCB2 (0x8000-0x81FF): Y zoom
    // SCB3 (0x8200-0x83FF): Y position, sticky bit, height
    // SCB4 (0x8400-0x85FF): X position, X zoom
    
    int yZoom = 0xFF;
    int spriteY = 0;
    int spriteHeight = 0;
    
    for (int bank = 0; bank < 381; bank++) {
        // Read SCB3 (Y position, sticky, height)
        u16 scb3 = readVRAM16(0x8200 + bank * 2);
        
        bool sticky = (scb3 & 0x40) != 0;
        
        if (!sticky) {
            // New sprite chain - read Y position and height
            spriteY = (0x200 - (scb3 >> 7)) & 0x1FF;
            spriteHeight = scb3 & 0x3F;
            
            // Read SCB2 (Y zoom)
            u16 scb2 = readVRAM16(0x8000 + bank * 2);
            yZoom = scb2 & 0xFF;
        }
        
        if (spriteHeight == 0) continue;
        
        // Read SCB4 (X position, X zoom)
        u16 scb4 = readVRAM16(0x8400 + bank * 2);
        int spriteX = scb4 >> 7;
        int xZoom = (scb4 >> 8) & 0x0F;
        
        // Adjust X for screen width
        if (spriteX >= 0x1E0) {
            spriteX -= 0x200;
        }
        
        // Render each tile row of this sprite bank
        for (int row = 0; row < spriteHeight && row < 32; row++) {
            // Read SCB1 (tile number + palette)
            u16 scb1 = readVRAM16(bank * 0x40 + row * 2);
            u16 scb1b = readVRAM16(bank * 0x40 + row * 2 + 0x8000 + 0x2000); // Second word for tile num high bits
            
            // Tile number is 20 bits
            u32 tileNum = (scb1 & 0xFFF) | ((scb1b & 0xF0) << 8) | ((scb1b & 0x0F) << 16);
            u32 palette = (scb1 >> 8) & 0xFF;
            bool flipX = (scb1b & 0x01) != 0;
            bool flipY = (scb1b & 0x02) != 0;
            
            // Auto-animation
            if (scb1b & 0x04) {
                tileNum = (tileNum & ~0x03) | (m_spriteFrame & 0x03);
            }
            if (scb1b & 0x08) {
                tileNum = (tileNum & ~0x07) | (m_spriteFrame & 0x07);
            }
            
            // Calculate Y position for this row
            int tileY = spriteY + row * 16;
            if (tileY >= 0x200) tileY -= 0x200;
            
            // Render the tile
            renderTile(tileNum, spriteX, tileY, palette, flipX, flipY, xZoom, yZoom);
        }
        
        // Move X position for next sprite in chain
        if (sticky) {
            // Sticky sprites are placed next to each other
        }
    }
}

void PPU::renderTile(u32 tileNum, int x, int y, u32 palette, bool flipX, bool flipY, int zoomX, [[maybe_unused]] int zoomY) {
    if (!m_cartridge) return;
    
    // Each tile is 16x16 pixels, stored as 128 bytes (4bpp = 4 bits per pixel)
    // Sprite ROM is organized as: 8 bytes per row, 16 rows per tile
    
    u32 tileOffset = tileNum * 128;  // 128 bytes per tile
    
    // Calculate zoom factors (0 = full size, 0xF = minimum)
    // For simplicity, we'll just render at full size initially
    int tileWidth = 16 - zoomX;
    
    for (int ty = 0; ty < 16; ty++) {
        int screenY = y + (flipY ? (15 - ty) : ty);
        if (screenY < 0 || screenY >= static_cast<int>(SCREEN_HEIGHT)) continue;
        
        // Each row is 8 bytes (4bpp, 16 pixels)
        u32 rowOffset = tileOffset + ty * 8;
        
        for (int tx = 0; tx < 16 && tx < tileWidth; tx++) {
            int screenX = x + (flipX ? (15 - tx) : tx);
            if (screenX < 0 || screenX >= static_cast<int>(SCREEN_WIDTH)) continue;
            
            // Get pixel color (4bpp)
            // Sprite ROM is interleaved: odd/even bytes
            u32 byteOffset = rowOffset + (tx / 2);
            u8 data = m_cartridge->readSpriteROM8(byteOffset);
            
            u8 colorIndex;
            if (tx & 1) {
                colorIndex = data >> 4;
            } else {
                colorIndex = data & 0x0F;
            }
            
            // Color 0 is transparent
            if (colorIndex == 0) continue;
            
            // Get color from palette
            u32 color = getPaletteColor(palette, colorIndex);
            
            // Write to frame buffer
            m_frameBuffer[screenY * SCREEN_WIDTH + screenX] = color;
        }
    }
}

void PPU::renderText() {
    if (!m_cartridge) return;
    
    // Fix layer (text layer) is a 40x32 tile map at VRAM 0xE000
    // Each entry is 16 bits: PPPP TTTT TTTT TTTT (P=palette, T=tile)
    // Tiles are 8x8 pixels
    // Layout: column-major, each column has 32 tiles
    
    for (int row = 2; row < 30; row++) {
        for (int col = 0; col < 40; col++) {
            // Read tilemap entry - column-major layout
            u16 entry = readVRAM16(0xE000 + row * 2 + col * 64);
            
            u32 tileNum = entry & 0x0FFF;
            u32 palette = (entry >> 12) & 0x0F;
            
            // Skip empty tiles (tile 0 is typically blank)
            if (tileNum == 0) continue;
            
            int x = col * 8;
            int y = (row - 2) * 8;  // Adjust for skipped border rows
            
            renderTextTile(tileNum, x, y, palette);
        }
    }
}

void PPU::renderTextTile(u32 tileNum, int x, int y, u32 palette) {
    if (!m_cartridge || !m_memory) return;
    
    // Text tiles are 8x8, 4bpp, stored as 32 bytes per tile (after decoding)
    // Format: 4 bytes per row, each byte = 2 pixels (high nibble first, then low nibble)
    u32 tileOffset = tileNum * 32;
    
    // Use BIOS text ROM or game text ROM depending on setting
    bool useBiosTextRom = m_memory->isBIOSTextROMEnabled();
    
    for (int ty = 0; ty < 8; ty++) {
        int screenY = y + ty;
        if (screenY < 0 || screenY >= static_cast<int>(SCREEN_HEIGHT)) continue;
        
        // Each row is 4 bytes (4bpp, 8 pixels)
        for (int tx = 0; tx < 8; tx++) {
            int screenX = x + tx;
            if (screenX < 0 || screenX >= static_cast<int>(SCREEN_WIDTH)) continue;
            
            // Get pixel data - text ROM is pre-decoded
            // Each byte has 2 pixels: high nibble = even pixel, low nibble = odd pixel
            u32 byteOffset = tileOffset + ty * 4 + (tx / 2);
            u8 data = useBiosTextRom ? m_cartridge->readBIOSText8(byteOffset) 
                                      : m_cartridge->readTextROM8(byteOffset);
            
            u8 colorIndex;
            if (tx & 1) {
                colorIndex = data & 0x0F;  // Odd pixel = low nibble
            } else {
                colorIndex = data >> 4;    // Even pixel = high nibble
            }
            
            // Color 0 is transparent
            if (colorIndex == 0) continue;
            
            // Text layer uses palettes 0-15 (same palette bank, not offset by 16)
            u32 color = getPaletteColor(palette, colorIndex);
            
            // Write to frame buffer
            m_frameBuffer[screenY * SCREEN_WIDTH + screenX] = color;
        }
    }
}

u32 PPU::convertPalette(u16 paletteEntry) const {
    // NeoGeo palette format:
    // Bit 15: Dark bit (reduces brightness)
    // Bit 14: Red LSB
    // Bit 13: Green LSB  
    // Bit 12: Blue LSB
    // Bits 11-8: Red high nibble
    // Bits 7-4: Green high nibble
    // Bits 3-0: Blue high nibble
    
    // Extract 6-bit color values
    int r = (paletteEntry & 0x0F00) >> 4;   // Red bits 11-8 -> bits 7-4
    r |= (paletteEntry >> 11) & 8;           // Red bit 14 -> bit 3
    r |= (paletteEntry >> 13) & 4;           // Dark bit 15 -> bit 2
    
    int g = (paletteEntry & 0x00F0);         // Green bits 7-4 -> bits 7-4
    g |= (paletteEntry >> 10) & 8;           // Green bit 13 -> bit 3
    g |= (paletteEntry >> 13) & 4;           // Dark bit 15 -> bit 2
    
    int b = (paletteEntry & 0x000F) << 4;    // Blue bits 3-0 -> bits 7-4
    b |= (paletteEntry >> 9) & 8;            // Blue bit 12 -> bit 3
    b |= (paletteEntry >> 13) & 4;           // Dark bit 15 -> bit 2
    
    // Expand 6-bit to 8-bit (simple method: shift left 2, copy top bits to bottom)
    r = (r << 2) | (r >> 4);
    g = (g << 2) | (g >> 4);
    b = (b << 2) | (b >> 4);
    
    return 0xFF000000 | (r << 16) | (g << 8) | b;
}

u32 PPU::getPaletteColor(u32 paletteIndex, u32 colorIndex) const {
    if (!m_memory) return 0xFF000000;
    
    // Each palette has 16 colors, each color is 2 bytes
    u32 address = (paletteIndex * 16 + colorIndex) * 2;
    u16 paletteEntry = m_memory->readPalette16(address);
    
    return convertPalette(paletteEntry);
}

void PPU::saveState(std::ofstream& file) {
    file.write(reinterpret_cast<const char*>(m_frameBuffer.data()), m_frameBuffer.size() * sizeof(u32));
    file.write(reinterpret_cast<const char*>(m_vram.data()), m_vram.size());
    file.write(reinterpret_cast<const char*>(&m_vramPointer), sizeof(m_vramPointer));
    file.write(reinterpret_cast<const char*>(&m_vramModulo), sizeof(m_vramModulo));
    file.write(reinterpret_cast<const char*>(&m_vramBank), sizeof(m_vramBank));
    file.write(reinterpret_cast<const char*>(&m_scanline), sizeof(m_scanline));
    file.write(reinterpret_cast<const char*>(&m_cycles), sizeof(m_cycles));
    file.write(reinterpret_cast<const char*>(&m_spriteFrame), sizeof(m_spriteFrame));
    file.write(reinterpret_cast<const char*>(&m_spriteFrameTimer), sizeof(m_spriteFrameTimer));
}

void PPU::loadState(std::ifstream& file) {
    file.read(reinterpret_cast<char*>(m_frameBuffer.data()), m_frameBuffer.size() * sizeof(u32));
    file.read(reinterpret_cast<char*>(m_vram.data()), m_vram.size());
    file.read(reinterpret_cast<char*>(&m_vramPointer), sizeof(m_vramPointer));
    file.read(reinterpret_cast<char*>(&m_vramModulo), sizeof(m_vramModulo));
    file.read(reinterpret_cast<char*>(&m_vramBank), sizeof(m_vramBank));
    file.read(reinterpret_cast<char*>(&m_scanline), sizeof(m_scanline));
    file.read(reinterpret_cast<char*>(&m_cycles), sizeof(m_cycles));
    file.read(reinterpret_cast<char*>(&m_spriteFrame), sizeof(m_spriteFrame));
    file.read(reinterpret_cast<char*>(&m_spriteFrameTimer), sizeof(m_spriteFrameTimer));
}

} // namespace neogeo
