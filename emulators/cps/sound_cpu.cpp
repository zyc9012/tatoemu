#include "sound_cpu.h"
#include "memory_base.h"
#include "apu_base.h"
#include "../components/cpu/z80/z80.h"
#include <iostream>
#include <cstring>

namespace cps {

// Static context pointer for callbacks
static SoundCPU* g_soundCpuContext = nullptr;

// Z80 callback functions for FBNeo Z80
extern "C" {
    UINT8 z80_read_prog(UINT32 address) {
        if (g_soundCpuContext) {
            MemoryBase* mem = g_soundCpuContext->getMemory();
            if (mem) {
                return mem->readZ80(static_cast<u16>(address));
            }
        }
        return 0xFF;
    }

    void z80_write_prog(UINT32 address, UINT8 value) {
        if (g_soundCpuContext) {
            MemoryBase* mem = g_soundCpuContext->getMemory();
            if (mem) {
                mem->writeZ80(static_cast<u16>(address), value);
            }
        }
    }

    UINT8 z80_read_io(UINT32 port) {
        if (g_soundCpuContext) {
            APUBase* apu = g_soundCpuContext->getAPU();
            if (apu) {
                return apu->readPort(static_cast<u16>(port));
            }
        }
        return 0xFF;
    }

    void z80_write_io(UINT32 port, UINT8 value) {
        if (g_soundCpuContext) {
            APUBase* apu = g_soundCpuContext->getAPU();
            if (apu) {
                apu->writePort(static_cast<u16>(port), value);
            }
        }
    }
}

SoundCPU::SoundCPU()
    : m_memory(nullptr)
    , m_apu(nullptr)
    , m_cycles(0) {
    // Initialize FBNeo Z80
    static bool z80_initialized = false;
    if (!z80_initialized) {
        Z80Init();
        z80_initialized = true;
    }
    
    // Set context for callbacks
    g_soundCpuContext = this;
    
    // Set up FBNeo Z80 handlers
    Z80SetProgramReadHandler(z80_read_prog);
    Z80SetProgramWriteHandler(z80_write_prog);
    Z80SetIOReadHandler(z80_read_io);
    Z80SetIOWriteHandler(z80_write_io);
    // Set opcode read handlers (required for ROP() and ARG())
    Z80SetCPUOpReadHandler(z80_read_prog);
    Z80SetCPUOpArgReadHandler(z80_read_prog);
    
    // Reset the CPU
    reset();
}

SoundCPU::~SoundCPU() {
    if (g_soundCpuContext == this) {
        g_soundCpuContext = nullptr;
    }
}

void SoundCPU::reset() {
    Z80Reset();
    m_cycles = 0;
}

void SoundCPU::step(u32 cycles) {
    if (cycles > 0) {
        INT32 executed = Z80Execute(static_cast<INT32>(cycles));
        m_cycles += static_cast<u32>(executed);
    }
}

void SoundCPU::irq(bool state) {
    Z80SetIrqLine(0, state ? Z80_ASSERT_LINE : Z80_CLEAR_LINE);
}

void SoundCPU::nmi() {
    Z80SetIrqLine(Z80_INPUT_LINE_NMI, Z80_ASSERT_LINE);
}

void SoundCPU::saveState(std::ofstream& file) {
    // Save Z80 state using FBNeo's context save
    Z80_Regs regs;
    Z80GetContext(&regs);
    
    file.write(reinterpret_cast<const char*>(&regs), sizeof(regs));
    file.write(reinterpret_cast<const char*>(&m_cycles), sizeof(m_cycles));
}

void SoundCPU::loadState(std::ifstream& file) {
    // Load Z80 state using FBNeo's context load
    Z80_Regs regs;
    file.read(reinterpret_cast<char*>(&regs), sizeof(regs));
    Z80SetContext(&regs);
    
    file.read(reinterpret_cast<char*>(&m_cycles), sizeof(m_cycles));
}

} // namespace cps
