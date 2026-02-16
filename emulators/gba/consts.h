#pragma once

#include "types.h"

namespace gba {

// GBA hardware specifications
constexpr u16 SCREEN_WIDTH = 240;
constexpr u16 SCREEN_HEIGHT = 160;
constexpr u32 CPU_FREQUENCY = 16777216; // 16.78 MHz (0x1000000)

// Video timing
constexpr u32 HDRAW_CYCLES = 1008;     // Cycles for visible pixels
constexpr u32 HBLANK_CYCLES = 224;     // Cycles for HBlank
constexpr u32 SCANLINE_CYCLES = HDRAW_CYCLES + HBLANK_CYCLES; // 1232 cycles per scanline
constexpr u32 VISIBLE_LINES = 160;
constexpr u32 VBLANK_LINES = 68;
constexpr u32 TOTAL_LINES = VISIBLE_LINES + VBLANK_LINES; // 228 lines
constexpr u32 CYCLES_PER_FRAME = SCANLINE_CYCLES * TOTAL_LINES; // 280896 cycles

constexpr double TARGET_FPS = static_cast<double>(CPU_FREQUENCY) / CYCLES_PER_FRAME; // ~59.7275 Hz

// Memory sizes
constexpr u32 BIOS_SIZE = 0x4000;      // 16 KB
constexpr u32 EWRAM_SIZE = 0x40000;    // 256 KB
constexpr u32 IWRAM_SIZE = 0x8000;     // 32 KB
constexpr u32 IO_SIZE = 0x400;         // 1 KB
constexpr u32 PALETTE_SIZE = 0x400;    // 1 KB
constexpr u32 VRAM_SIZE = 0x18000;     // 96 KB
constexpr u32 OAM_SIZE = 0x400;        // 1 KB
constexpr u32 MAX_ROM_SIZE = 0x2000000; // 32 MB
constexpr u32 SRAM_SIZE = 0x10000;     // 64 KB (max SRAM/Flash)

// Memory regions (top nibble of address)
constexpr u32 REGION_BIOS = 0x0;
constexpr u32 REGION_EWRAM = 0x2;
constexpr u32 REGION_IWRAM = 0x3;
constexpr u32 REGION_IO = 0x4;
constexpr u32 REGION_PALETTE = 0x5;
constexpr u32 REGION_VRAM = 0x6;
constexpr u32 REGION_OAM = 0x7;
constexpr u32 REGION_ROM0 = 0x8;
constexpr u32 REGION_ROM0H = 0x9;
constexpr u32 REGION_ROM1 = 0xA;
constexpr u32 REGION_ROM1H = 0xB;
constexpr u32 REGION_ROM2 = 0xC;
constexpr u32 REGION_ROM2H = 0xD;
constexpr u32 REGION_SRAM = 0xE;
constexpr u32 REGION_SRAM_MIRROR = 0xF;

// IO Register addresses (offset from 0x04000000)
namespace IO {
    // LCD
    constexpr u32 DISPCNT   = 0x000;
    constexpr u32 GREENSWAP = 0x002;
    constexpr u32 DISPSTAT  = 0x004;
    constexpr u32 VCOUNT    = 0x006;
    constexpr u32 BG0CNT    = 0x008;
    constexpr u32 BG1CNT    = 0x00A;
    constexpr u32 BG2CNT    = 0x00C;
    constexpr u32 BG3CNT    = 0x00E;
    constexpr u32 BG0HOFS   = 0x010;
    constexpr u32 BG0VOFS   = 0x012;
    constexpr u32 BG1HOFS   = 0x014;
    constexpr u32 BG1VOFS   = 0x016;
    constexpr u32 BG2HOFS   = 0x018;
    constexpr u32 BG2VOFS   = 0x01A;
    constexpr u32 BG3HOFS   = 0x01C;
    constexpr u32 BG3VOFS   = 0x01E;
    constexpr u32 BG2PA     = 0x020;
    constexpr u32 BG2PB     = 0x022;
    constexpr u32 BG2PC     = 0x024;
    constexpr u32 BG2PD     = 0x026;
    constexpr u32 BG2X      = 0x028;
    constexpr u32 BG2X_H    = 0x02A;
    constexpr u32 BG2Y      = 0x02C;
    constexpr u32 BG2Y_H    = 0x02E;
    constexpr u32 BG3PA     = 0x030;
    constexpr u32 BG3PB     = 0x032;
    constexpr u32 BG3PC     = 0x034;
    constexpr u32 BG3PD     = 0x036;
    constexpr u32 BG3X      = 0x038;
    constexpr u32 BG3X_H    = 0x03A;
    constexpr u32 BG3Y      = 0x03C;
    constexpr u32 BG3Y_H    = 0x03E;
    constexpr u32 WIN0H     = 0x040;
    constexpr u32 WIN1H     = 0x042;
    constexpr u32 WIN0V     = 0x044;
    constexpr u32 WIN1V     = 0x046;
    constexpr u32 WININ     = 0x048;
    constexpr u32 WINOUT    = 0x04A;
    constexpr u32 MOSAIC    = 0x04C;
    constexpr u32 BLDCNT    = 0x050;
    constexpr u32 BLDALPHA  = 0x052;
    constexpr u32 BLDY      = 0x054;

    // DMA
    constexpr u32 DMA0SAD   = 0x0B0;
    constexpr u32 DMA0DAD   = 0x0B4;
    constexpr u32 DMA0CNT_L = 0x0B8;
    constexpr u32 DMA0CNT_H = 0x0BA;
    constexpr u32 DMA1SAD   = 0x0BC;
    constexpr u32 DMA1DAD   = 0x0C0;
    constexpr u32 DMA1CNT_L = 0x0C4;
    constexpr u32 DMA1CNT_H = 0x0C6;
    constexpr u32 DMA2SAD   = 0x0C8;
    constexpr u32 DMA2DAD   = 0x0CC;
    constexpr u32 DMA2CNT_L = 0x0D0;
    constexpr u32 DMA2CNT_H = 0x0D2;
    constexpr u32 DMA3SAD   = 0x0D4;
    constexpr u32 DMA3DAD   = 0x0D8;
    constexpr u32 DMA3CNT_L = 0x0DC;
    constexpr u32 DMA3CNT_H = 0x0DE;

    // Sound
    constexpr u32 SOUND1CNT_L = 0x060;
    constexpr u32 SOUND1CNT_H = 0x062;
    constexpr u32 SOUND1CNT_X = 0x064;
    constexpr u32 SOUND2CNT_L = 0x068;
    constexpr u32 SOUND2CNT_H = 0x06C;
    constexpr u32 SOUND3CNT_L = 0x070;
    constexpr u32 SOUND3CNT_H = 0x072;
    constexpr u32 SOUND3CNT_X = 0x074;
    constexpr u32 SOUND4CNT_L = 0x078;
    constexpr u32 SOUND4CNT_H = 0x07C;
    constexpr u32 SOUNDCNT_L  = 0x080;
    constexpr u32 SOUNDCNT_H  = 0x082;
    constexpr u32 SOUNDCNT_X  = 0x084;
    constexpr u32 SOUNDBIAS   = 0x088;
    constexpr u32 WAVE_RAM    = 0x090;
    constexpr u32 FIFO_A      = 0x0A0;
    constexpr u32 FIFO_B      = 0x0A4;

    // Timers
    constexpr u32 TM0CNT_L  = 0x100;
    constexpr u32 TM0CNT_H  = 0x102;
    constexpr u32 TM1CNT_L  = 0x104;
    constexpr u32 TM1CNT_H  = 0x106;
    constexpr u32 TM2CNT_L  = 0x108;
    constexpr u32 TM2CNT_H  = 0x10A;
    constexpr u32 TM3CNT_L  = 0x10C;
    constexpr u32 TM3CNT_H  = 0x10E;

    // Keypad
    constexpr u32 KEYINPUT  = 0x130;
    constexpr u32 KEYCNT    = 0x132;

    // Interrupts
    constexpr u32 IE        = 0x200;
    constexpr u32 IF        = 0x202;
    constexpr u32 WAITCNT   = 0x204;
    constexpr u32 IME       = 0x208;
    constexpr u32 POSTFLG   = 0x300;
    constexpr u32 HALTCNT   = 0x301;
}

// IRQ bits
namespace IRQ {
    constexpr u16 VBLANK   = 1 << 0;
    constexpr u16 HBLANK   = 1 << 1;
    constexpr u16 VCOUNTER = 1 << 2;
    constexpr u16 TIMER0   = 1 << 3;
    constexpr u16 TIMER1   = 1 << 4;
    constexpr u16 TIMER2   = 1 << 5;
    constexpr u16 TIMER3   = 1 << 6;
    constexpr u16 SERIAL   = 1 << 7;
    constexpr u16 DMA0     = 1 << 8;
    constexpr u16 DMA1     = 1 << 9;
    constexpr u16 DMA2     = 1 << 10;
    constexpr u16 DMA3     = 1 << 11;
    constexpr u16 KEYPAD   = 1 << 12;
    constexpr u16 GAMEPAK  = 1 << 13;
}

// DISPSTAT bits
namespace DISPSTAT {
    constexpr u16 VBLANK_FLAG   = 1 << 0;
    constexpr u16 HBLANK_FLAG   = 1 << 1;
    constexpr u16 VCOUNTER_FLAG = 1 << 2;
    constexpr u16 VBLANK_IRQ    = 1 << 3;
    constexpr u16 HBLANK_IRQ    = 1 << 4;
    constexpr u16 VCOUNTER_IRQ  = 1 << 5;
    constexpr u16 VCOUNT_SETTING_MASK = 0xFF00;
    constexpr int VCOUNT_SETTING_SHIFT = 8;
}

// DMA timing
namespace DMA_TIMING {
    constexpr int IMMEDIATE = 0;
    constexpr int VBLANK = 1;
    constexpr int HBLANK = 2;
    constexpr int SPECIAL = 3;
}

// Timer prescaler values
constexpr int TIMER_PRESCALER[] = {1, 64, 256, 1024};

// Initial stack pointers
constexpr u32 SP_USR = 0x03007F00;
constexpr u32 SP_IRQ = 0x03007FA0;
constexpr u32 SP_SVC = 0x03007FE0;

// Save types
enum class SaveType {
    NONE,
    SRAM,
    FLASH_64K,
    FLASH_128K,
    EEPROM_512,
    EEPROM_8K,
};

} // namespace gba
