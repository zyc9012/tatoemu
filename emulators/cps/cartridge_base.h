#pragma once

#include "../types.h"
#include <filesystem>
#include <string>
#include <fstream>

namespace cps {

class CPU;
class PPUBase;

// Base class for ROM/Cartridge loader (shared interface)
class CartridgeBase {
public:
    virtual ~CartridgeBase() = default;

    virtual bool load(const fs::path& filename) = 0;
    virtual void reset() = 0;
    
    virtual const std::string& getTitle() const = 0;
    
    // ROM access
    virtual u8 readROM8(u32 address) = 0;
    virtual u16 readROM16(u32 address) = 0;
    virtual u32 readROM32(u32 address) = 0;
    
    // Component connections
    virtual void setCPU(CPU* cpu) = 0;
    virtual void setPPU(PPUBase* ppu) = 0;
    
    // Save/Load state
    virtual void saveState(std::ofstream& file) = 0;
    virtual void loadState(std::ifstream& file) = 0;
};

} // namespace cps
