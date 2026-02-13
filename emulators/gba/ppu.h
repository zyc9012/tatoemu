#pragma once

#include "types.h"
#include "consts.h"
#include "../components/buffer.h"

namespace gba {

class Memory;
class DMA;

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
    
    // Rendering helpers
    void renderTextBG(int bg, int y, u16 dispcnt, u8* palette, u8* vram, u32* line, u8* priorityBuf);
    void renderAffineBG(int bg, int y, u16 dispcnt, u8* palette, u8* vram, u32* line, u8* priorityBuf);
    void renderBitmapMode3(int y, u8* vram, u32* line, u8* priorityBuf);
    void renderBitmapMode4(int y, u16 dispcnt, u8* palette, u8* vram, u32* line, u8* priorityBuf);
    void renderBitmapMode5(int y, u16 dispcnt, u8* vram, u32* line, u8* priorityBuf);
    void renderSprites(int y, u16 dispcnt, u8* palette, u8* vram, u8* oam, u32* line, u8* priorityBuf);
    u8 getObjPixel(int tileIndex, int x, int y, int objWidth, bool is8bpp, bool mapping1D, u8* objVram);
    
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
