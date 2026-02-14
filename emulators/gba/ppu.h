#pragma once

#include "types.h"
#include "consts.h"
#include "../components/buffer.h"
#include <algorithm>

namespace gba {

class Memory;
class DMA;

// Layer identifiers for blending target checks
enum Layer : u8 {
    LAYER_BG0 = 0,
    LAYER_BG1 = 1,
    LAYER_BG2 = 2,
    LAYER_BG3 = 3,
    LAYER_OBJ = 4,
    LAYER_BD  = 5,
};

// Per-pixel compositing data
struct ScanPixel {
    u16 color;     // RGB555
    u8  layer;     // Layer enum
    u8  priority;  // 0-3 (BG/OBJ), 4 (backdrop)
};

class PPU {
public:
    PPU();
    ~PPU();

    void setMemory(Memory* memory) { m_memory = memory; }
    void setDMA(DMA* dma) { m_dma = dma; }
    void setVideoDevice(VideoDevice* device) { m_videoDevice = device; }
    
    void reset();
    void step(int cycles);
    
    void writeRegister(u32 offset, u16 value);
    u16 getVCount() const { return m_vcount; }
    
    // Save/Load state
    void saveState(Buffer* buf);
    void loadState(Buffer* buf);

private:
    void renderScanline();
    void enterHBlank();
    void enterVBlank();
    
    // Pixel placement for two-layer compositing
    static inline void placePixel(ScanPixel* top, ScanPixel* bot, int x,
                                  u16 color, u8 layer, u8 priority) {
        if (priority <= top[x].priority) {
            bot[x] = top[x];
            top[x] = { color, layer, priority };
        } else if (priority <= bot[x].priority) {
            bot[x] = { color, layer, priority };
        }
    }
    
    // Rendering helpers
    void renderTextBG(int bg, int y, u16 dispcnt, u8* palette, u8* vram,
                      ScanPixel* top, ScanPixel* bot);
    void renderAffineBG(int bg, int y, u16 dispcnt, u8* palette, u8* vram,
                        ScanPixel* top, ScanPixel* bot);
    void renderBitmapMode3(int y, u8* vram,
                           ScanPixel* top, ScanPixel* bot);
    void renderBitmapMode4(int y, u16 dispcnt, u8* palette, u8* vram,
                           ScanPixel* top, ScanPixel* bot);
    void renderBitmapMode5(int y, u16 dispcnt, u8* vram,
                           ScanPixel* top, ScanPixel* bot);
    void renderSprites(int y, u16 dispcnt, u8* palette, u8* vram, u8* oam,
                       ScanPixel* top, ScanPixel* bot, bool* objSemiTransparent);
    u8 getObjPixel(int tileIndex, int x, int y, int objWidth, bool is8bpp, bool mapping1D, u8* objVram);
    
    // Blending / color special effects
    void composeScanline(u32* line, ScanPixel* top, ScanPixel* bot, bool* objSemiTransparent);
    
    Memory* m_memory = nullptr;
    DMA* m_dma = nullptr;
    VideoDevice* m_videoDevice = nullptr;
    
    u32 m_framebuffer[SCREEN_WIDTH * SCREEN_HEIGHT];
    
    u16 m_vcount = 0;
    u32 m_cycles = 0;
    bool m_inHBlank = false;
    bool m_inVBlank = false;
};

} // namespace gba
