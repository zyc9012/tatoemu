#pragma once

#include "types.h"
#include "consts.h"
#include "../components/buffer.h"
#include <cstdint>

namespace gba {

class Cartridge;
class PPU;
class Joypad;
class Timer;
class DMA;
class APU;
class GPIO;

class Memory {
public:
    Memory();
    ~Memory();

    void setCartridge(Cartridge* cart) { m_cartridge = cart; }
    void setPPU(PPU* ppu) { m_ppu = ppu; }
    void setJoypad(Joypad* joypad) { m_joypad = joypad; }
    void setTimer(Timer* timer) { m_timer = timer; }
    void setDMA(DMA* dma) { m_dma = dma; }
    void setAPU(APU* apu) { m_apu = apu; }
    void setGPIO(GPIO* gpio) { m_gpio = gpio; }
    
    Cartridge* getCartridge() const { return m_cartridge; }

    void reset();
    bool loadBIOS(const u8* data, u32 size);
    bool hasBIOS() const { return m_hasBIOS; }

    // Halt state (set by BIOS HLE or HALTCNT writes)
    bool isHalted() const { return m_halted; }
    void setHalted(bool halted) { m_halted = halted; }

    // Memory access
    u8 read8(u32 address);
    u16 read16(u32 address);
    u32 read32(u32 address);
    void write8(u32 address, u8 value);
    void write16(u32 address, u16 value);
    void write32(u32 address, u32 value);

    // Instruction fetch (separate for HLE interception)
    u16 fetch16(u32 address);
    u32 fetch32(u32 address);

    // Direct memory access (for DMA, PPU, etc.)
    u8* getEWRAM() { return m_ewram; }
    u8* getIWRAM() { return m_iwram; }
    u8* getPalette() { return m_palette; }
    u8* getVRAM() { return m_vram; }
    u8* getOAM() { return m_oam; }
    u8* getIO() { return m_io; }

    // IO register helpers
    u16 readIO16(u32 offset) const;
    void writeIO16(u32 offset, u16 value);
    u32 readIO32(u32 offset) const;

    // IRQ management
    void requestIRQ(u16 irqBit);

    // Wait state cycle accumulation (consumed by CPU after each step)
    int consumeWaitCycles() { int c = m_waitCycles; m_waitCycles = 0; return c; }

    // Save/Load state
    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

private:
    u8 readIO(u32 address);
    void writeIO(u32 address, u8 value);
    void updateWaitstates(u16 waitcnt);
    void updateEWRAMWaitstates(u16 value);
    void fillPrefetch(int availableCycles);
    void prefetchStep(u32 region, int accessWait);

    Cartridge* m_cartridge = nullptr;
    PPU* m_ppu = nullptr;
    Joypad* m_joypad = nullptr;
    Timer* m_timer = nullptr;
    DMA* m_dma = nullptr;
    APU* m_apu = nullptr;
    GPIO* m_gpio = nullptr;

    u8 m_bios[BIOS_SIZE];
    u8 m_ewram[EWRAM_SIZE];
    u8 m_iwram[IWRAM_SIZE];
    u8 m_io[IO_SIZE];
    u8 m_palette[PALETTE_SIZE];
    u8 m_vram[VRAM_SIZE];
    u8 m_oam[OAM_SIZE];

    bool m_hasBIOS = false;
    bool m_halted = false;
    u32 m_openBus = 0;

    // Wait states
    u32 m_exWaitcnt = 0;           // EXWAITCNT register (0x04000800)
    int m_waitCycles = 0;          // Accumulated extra wait cycles
    int m_wsNonseq16[16] = {};     // Non-sequential 16-bit wait states per region
    int m_wsNonseq32[16] = {};     // Non-sequential 32-bit wait states per region
    int m_wsSeq16[16] = {};        // Sequential 16-bit wait states per region
    int m_wsSeq32[16] = {};        // Sequential 32-bit wait states per region

    // Prefetch buffer
    bool m_prefetchEnabled = false;  // WAITCNT bit 14
    bool m_isFetch = false;          // True during instruction fetch (guards prefetchStep)
    int  m_prefetchCount = 0;        // Halfwords buffered (0-8)
    u32  m_prefetchHeadAddr = 0;     // Address of first buffered halfword
    u32  m_fetchRegion = 0;          // Region the CPU is currently executing from
    u32  m_lastFetchAddr = ~0u;      // Previous instruction fetch address
};

} // namespace gba
