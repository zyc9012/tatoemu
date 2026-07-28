#pragma once

#include "consts.h"
#include "../types.h"
#include "../components/buffer.h"
#include <array>

namespace md {

class CPU;
class Memory;

// ---------------------------------------------------------------------------
// Sega 315-5313 VDP (Mode 5 only)
//
// Implements the two scroll planes, the window plane, the sprite engine with
// per-line limits and masking, shadow/highlight, all three DMA modes and the
// H/V interrupt sources.
// ---------------------------------------------------------------------------
class VDP {
public:
    VDP();
    ~VDP() = default;

    void reset();

    void setCPU(CPU* cpu) { m_cpu = cpu; }
    void setMemory(Memory* memory) { m_memory = memory; }
    void setVideoDevice(::VideoDevice* device) { m_videoDevice = device; }
    void setPAL(bool pal);

    // --- 68000 / Z80 port interface (0xC00000-0xC0000F) ---
    u16  readData();
    void writeData(u16 value);
    u16  readControl();
    void writeControl(u16 value);
    u16  readHVCounter() const;

    // Called from the 68000 interrupt acknowledge cycle.  The VDP holds both
    // interrupt lines until the CPU acknowledges them, and only the level that
    // is actually being serviced is retired.
    void acknowledgeIRQ(u8 level);

    // --- Frame sequencing, driven by Core ---
    void beginFrame();
    void beginLine(u32 line);
    // Called when the active portion of the line has elapsed: renders the line
    // and evaluates the H/V interrupt sources.
    void endActiveDisplay(u32 line);
    void endFrame();

    // True once per frame, on the line where the vertical interrupt fires.
    // Core uses this to pulse the Z80 interrupt line.
    bool consumeVIntEvent() { bool v = m_vintEvent; m_vintEvent = false; return v; }

    bool isPAL() const { return m_pal; }
    bool isH40() const { return (m_regs[12] & 0x81) == 0x81; }
    u32  totalScanlines() const { return m_pal ? PAL_TOTAL_SCANLINES : NTSC_TOTAL_SCANLINES; }
    u32  activeScanlines() const { return (m_regs[1] & 0x08) ? 240u : 224u; }
    double targetFPS() const { return m_pal ? TARGET_FPS_PAL : TARGET_FPS_NTSC; }

    // Cycle window of the scanline being emulated.  The H counter interpolates
    // the 68000's position inside it so that software which busy-waits on a
    // particular counter value sees it advance.
    void setLineWindow(u32 startCycle, u32 cycles) {
        m_lineStartCycle = startCycle;
        m_lineCycles = cycles ? cycles : 1;
    }

    u8* getVRAM() { return m_vram.data(); }

    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

private:
    // --- register / port helpers ---
    void writeRegister(u8 index, u8 value);
    void updateIRQ();
    void writeDataInternal(u16 value);
    void bumpAddress();
    u32  dmaLength() const;

    // --- DMA ---
    void startDMA();
    void dmaMemoryToVdp();
    void dmaVramFill(u16 value);
    void dmaVramCopy();

    // --- rendering ---
    void renderLine(u32 line);
    void renderPlane(bool planeB, u32 line, u8* dest, u32 xStart, u32 xEnd);
    void renderWindow(u32 line, u8* dest, u32 xStart, u32 xEnd);
    void renderSprites(u32 line);
    void composite(u32 line);
    void updatePaletteEntry(u32 index);

    u16 readVRAM16(u32 address) const {
        address &= 0xFFFE;
        return static_cast<u16>((m_vram[address] << 8) | m_vram[address + 1]);
    }

    CPU* m_cpu = nullptr;
    Memory* m_memory = nullptr;
    ::VideoDevice* m_videoDevice = nullptr;

    // --- memories ---
    std::array<u8, VRAM_SIZE> m_vram{};
    std::array<u16, CRAM_ENTRIES> m_cram{};
    std::array<u16, VSRAM_ENTRIES> m_vsram{};
    std::array<u8, 24> m_regs{};

    // Pre-shaded ARGB lookups for the three intensity levels.
    std::array<u32, CRAM_ENTRIES> m_paletteNormal{};
    std::array<u32, CRAM_ENTRIES> m_paletteShadow{};
    std::array<u32, CRAM_ENTRIES> m_paletteHighlight{};

    // --- control port state ---
    u32  m_addr = 0;
    u8   m_code = 0;
    bool m_pending = false;
    u16  m_readBuffer = 0;
    bool m_dmaFillPending = false;

    // --- status / timing ---
    u16  m_status = 0x3400;
    u32  m_line = 0;
    s32  m_hintCounter = 0;
    bool m_vintPending = false;
    bool m_hintPending = false;
    bool m_vintEvent = false;
    bool m_pal = false;
    bool m_oddFrame = false;

    u32 m_lineStartCycle = 0;
    u32 m_lineCycles = 1;

    // --- rendering scratch ---
    // Each entry: bit 6 = priority, bits 5-0 = palette<<4 | colour index.
    // Zero means transparent.
    std::array<u8, H40_WIDTH> m_planeA{};
    std::array<u8, H40_WIDTH> m_planeB{};
    std::array<u8, H40_WIDTH> m_sprites{};

    std::array<u32, SCREEN_WIDTH * SCREEN_HEIGHT> m_framebuffer{};
};

} // namespace md
