#pragma once

#include "../types.h"
#include <fstream>

namespace cps {

class CPU;
class CartridgeBase;

// Base class for Picture Processing Unit (shared interface)
class PPUBase {
public:
    virtual ~PPUBase() = default;

    virtual void reset() = 0;
    virtual void step() = 0;
    
    virtual bool isFrameComplete() const = 0;
    virtual void clearFrameComplete() = 0;
    
    virtual void setCPU(CPU* cpu) = 0;
    virtual void setCartridge(CartridgeBase* cartridge) = 0;
    virtual void setVideoDevice(::VideoDevice* videoDevice) = 0;
    
    // Save/Load state
    virtual void saveState(std::ofstream& file) = 0;
    virtual void loadState(std::ifstream& file) = 0;
};

} // namespace cps
