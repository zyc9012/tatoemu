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
    void updateDispstat();
    
    // Pixel placement for two-layer compositing
    // Returns true if the pixel was placed on top (for semi-transparency tracking)
    static inline bool placePixel(ScanPixel* top, ScanPixel* bot, int x,
                                  u16 color, u8 layer, u8 priority) {
        if (priority <= top[x].priority) {
            // Only push old top to bot if it's a different layer;
            // same-layer pixels (e.g. two OBJ sprites) must not occupy
            // both top and bot, since GBA blending only works between
            // different layers.
            if (layer != top[x].layer) {
                bot[x] = top[x];
            }
            top[x] = { color, layer, priority };
            return true;
        } else if (layer != top[x].layer && priority <= bot[x].priority) {
            bot[x] = { color, layer, priority };
        }
        return false;
    }
    
    // Window support
    // Window flags per pixel: bit layout matches WININ/WINOUT
    //   bit 0: BG0, bit 1: BG1, bit 2: BG2, bit 3: BG3, bit 4: OBJ, bit 5: SFX
    void computeWindowFlags(u16 dispcnt, int y, u8* oam, u8* vram, u8* windowFlags);
    
    // Rendering helpers
    void renderTextBG(int bg, int y, [[maybe_unused]] u16 dispcnt, u8* palette, u8* vram,
                      ScanPixel* top, ScanPixel* bot, const u8* windowFlags,
                      int bgMosaicH, int bgMosaicV);
    void renderAffineBG(int bg, int y, [[maybe_unused]] u16 dispcnt, u8* palette, u8* vram,
                        ScanPixel* top, ScanPixel* bot, const u8* windowFlags,
                        int bgMosaicH, int bgMosaicV);
    void renderBitmapMode3(int y, u8* vram,
                           ScanPixel* top, ScanPixel* bot, const u8* windowFlags,
                           int bgMosaicH, int bgMosaicV);
    void renderBitmapMode4(int y, u16 dispcnt, u8* palette, u8* vram,
                           ScanPixel* top, ScanPixel* bot, const u8* windowFlags,
                           int bgMosaicH, int bgMosaicV);
    void renderBitmapMode5(int y, u16 dispcnt, u8* vram,
                           ScanPixel* top, ScanPixel* bot, const u8* windowFlags,
                           int bgMosaicH, int bgMosaicV);
    void renderSprites(int y, u16 dispcnt, u8* palette, u8* vram, u8* oam,
                       ScanPixel* top, ScanPixel* bot, bool* objSemiTransparent,
                       const u8* windowFlags, int objMosaicH, int objMosaicV);
    u8 getObjPixel(int tileIndex, int x, int y, int objWidth, bool is8bpp, bool mapping1D, u8* objVram);
    
    // OBJ window mask (which pixels have OBJ window sprites)
    void computeObjWindowMask(int y, u16 dispcnt, u8* oam, u8* vram, bool* mask);
    
    // Blending / color special effects
    void composeScanline(u32* line, ScanPixel* top, ScanPixel* bot,
                         bool* objSemiTransparent, const u8* windowFlags);
    
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
