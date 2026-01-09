#pragma once

#include "../types.h"
#include <fstream>

namespace cps {

class CPU;
class SoundCPU;
class PPUBase;
class APUBase;
class CartridgeBase;
class Controller;

// Base class for Memory management (shared interface)
class MemoryBase {
public:
    virtual ~MemoryBase() = default;

    virtual void reset() = 0;
    
    // 68000 memory access
    virtual u8 read8(u32 address) = 0;
    virtual u16 read16(u32 address) = 0;
    virtual u32 read32(u32 address) = 0;
    virtual void write8(u32 address, u8 value) = 0;
    virtual void write16(u32 address, u16 value) = 0;
    virtual void write32(u32 address, u32 value) = 0;
    
    // Z80 memory access
    virtual u8 readZ80(u16 address) = 0;
    virtual void writeZ80(u16 address, u8 value) = 0;
    
    // Component connections
    virtual void setCPU(CPU* cpu) = 0;
    virtual void setSoundCPU(SoundCPU* soundCpu) = 0;
    virtual void setPPU(PPUBase* ppu) = 0;
    virtual void setAPU(APUBase* apu) = 0;
    virtual void setCartridge(CartridgeBase* cartridge) = 0;
    virtual void setController1(Controller* controller) = 0;
    virtual void setController2(Controller* controller) = 0;
    
    // Save/Load state
    virtual void saveState(std::ofstream& file) = 0;
    virtual void loadState(std::ifstream& file) = 0;
};

} // namespace cps
