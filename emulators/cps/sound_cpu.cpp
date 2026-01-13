#include "sound_cpu.h"
#include "memory.h"
#include "apu.h"
#include "../components/cpu/z80/z80.h"
#include <iostream>
#include <cstring>
#include <vector>
#include <cstddef>

namespace cps {

// Static context pointer for callbacks
static SoundCPU* g_soundCpuContext = nullptr;

// Z80 callback functions for Z80
extern "C" {
    u8 z80_read_prog(u32 address) {
        if (g_soundCpuContext) {
            Memory* mem = g_soundCpuContext->getMemory();
            if (mem) {
                return mem->readZ80(static_cast<u16>(address));
            }
        }
        return 0xFF;
    }

    void z80_write_prog(u32 address, u8 value) {
        if (g_soundCpuContext) {
            Memory* mem = g_soundCpuContext->getMemory();
            if (mem) {
                mem->writeZ80(static_cast<u16>(address), value);
            }
        }
    }

    u8 z80_read_io(u32 port) {
        if (g_soundCpuContext) {
            APU* apu = g_soundCpuContext->getAPU();
            if (apu) {
                return apu->readPort(static_cast<u16>(port));
            }
        }
        return 0xFF;
    }

    void z80_write_io(u32 port, u8 value) {
        if (g_soundCpuContext) {
            APU* apu = g_soundCpuContext->getAPU();
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
    // Initialize Z80
    static bool z80_initialized = false;
    if (!z80_initialized) {
        Z80Init();
        z80_initialized = true;
    }
    
    // Set context for callbacks
    g_soundCpuContext = this;
    
    // Set up Z80 handlers
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
        s32 executed = Z80Execute(static_cast<s32>(cycles));
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
    // Save Z80 state
    Z80_Regs regs;
    Z80GetContext(&regs);
    
    // Calculate size without function pointers
    size_t sizeNoPointers = offsetof(Z80_Regs, irq_callback);
    
    file.write(reinterpret_cast<const char*>(&sizeNoPointers), sizeof(sizeNoPointers));
    file.write(reinterpret_cast<const char*>(&regs), sizeNoPointers);
    file.write(reinterpret_cast<const char*>(&m_cycles), sizeof(m_cycles));
}

void SoundCPU::loadState(std::ifstream& file) {
    // Load Z80 state
    size_t sizeNoPointers;
    file.read(reinterpret_cast<char*>(&sizeNoPointers), sizeof(sizeNoPointers));
    
    if (sizeNoPointers != offsetof(Z80_Regs, irq_callback)) {
        std::cerr << "Error: Saved Z80 context size mismatch" << std::endl;
        return;
    }
    
    // Load saved context (without pointers)
    std::vector<char> savedContext(sizeNoPointers);
    file.read(savedContext.data(), sizeNoPointers);
    
    // Get current context
    Z80_Regs currentRegs;
    Z80GetContext(&currentRegs);
    
    // Copy saved context to current context, only copy the non-pointer portion
    memcpy(&currentRegs, savedContext.data(), sizeNoPointers);
    
    // Set current context back
    Z80SetContext(&currentRegs);
    
    file.read(reinterpret_cast<char*>(&m_cycles), sizeof(m_cycles));
}

} // namespace cps
