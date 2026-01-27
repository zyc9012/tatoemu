#pragma once

#include "../types.h"
#include "consts.h"
#include <array>
#include <fstream>

namespace nes {

class CPU;
class PPU;
class APU;
class Cartridge;
class Controller;

// Memory bus - handles address decoding and component routing
class Memory {
public:
    Memory();
    ~Memory() = default;

    void setCPU(CPU* cpu) { m_cpu = cpu; }
    void setPPU(PPU* ppu) { m_ppu = ppu; }
    void setAPU(APU* apu) { m_apu = apu; }
    void setCartridge(Cartridge* cartridge) { m_cartridge = cartridge; }
    void setController(Controller* controller) { m_controller = controller; }
    
    void reset();
    
    // CPU bus access
    u8 cpuRead(u16 address);
    void cpuWrite(u16 address, u8 value);
    
    // Direct RAM access (for debugging/save states)
    u8* getRAM() { return m_ram.data(); }
    
    // Save/Load state
    void saveState(std::ofstream& file) const;
    void loadState(std::ifstream& file);

private:
    CPU* m_cpu;
    PPU* m_ppu;
    APU* m_apu;
    Cartridge* m_cartridge;
    Controller* m_controller;
    
    // Internal RAM (2KB, mirrored 4 times to fill 8KB)
    std::array<u8, RAM_SIZE> m_ram;
    
    // Controller state
    bool m_controllerStrobe;
};

} // namespace nes
