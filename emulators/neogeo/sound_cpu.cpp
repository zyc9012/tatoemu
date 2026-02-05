#include "sound_cpu.h"
#include "memory.h"
#include "apu.h"
#include "../components/cpu/z80/z80.h"
#include <cstring>
#include <vector>
#include <cstddef>

namespace neogeo {

// Static context pointer for callbacks
static SoundCPU* g_soundCpuContext = nullptr;

// Z80 callback functions (static to avoid symbol conflicts with other emulators)
static u8 z80_read_prog(u32 address) {
    if (g_soundCpuContext) {
        Memory* mem = g_soundCpuContext->getMemory();
        if (mem) {
            return mem->readZ80(address);
        }
    }
    return 0xFF;
}

static void z80_write_prog(u32 address, u8 value) {
    if (g_soundCpuContext) {
        Memory* mem = g_soundCpuContext->getMemory();
        if (mem) {
            mem->writeZ80(address, value);
        }
    }
}

static u8 z80_read_io(u32 port) {
    if (g_soundCpuContext) {
        Memory* mem = g_soundCpuContext->getMemory();
        if (mem) {
            return mem->readZ80IO(static_cast<u16>(port));
        }
    }
    return 0xFF;
}

static void z80_write_io(u32 port, u8 value) {
    if (g_soundCpuContext) {
        Memory* mem = g_soundCpuContext->getMemory();
        if (mem) {
            mem->writeZ80IO(static_cast<u16>(port), value);
        }
    }
}

static u8 z80_read_op(u32 address) {
    if (g_soundCpuContext) {
        Memory* mem = g_soundCpuContext->getMemory();
        if (mem) {
            return mem->readZ80(address);
        }
    }
    return 0xFF;
}

static u8 z80_read_op_arg(u32 address) {
    if (g_soundCpuContext) {
        Memory* mem = g_soundCpuContext->getMemory();
        if (mem) {
            return mem->readZ80(address);
        }
    }
    return 0xFF;
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
    Z80SetCPUOpReadHandler(z80_read_op);
    Z80SetCPUOpArgReadHandler(z80_read_op_arg);
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

u32 SoundCPU::step(u32 cycles) {
    if (cycles > 0) {
        s32 executed = Z80Execute(static_cast<s32>(cycles));
        u32 actualCycles = static_cast<u32>(executed);
        m_cycles += actualCycles;

        // Update YM2610 timers - this will trigger IRQs when timers expire
        if (m_apu) {
            m_apu->updateTimers(actualCycles);
        }

        return actualCycles;
    }
    return 0;
}

void SoundCPU::irq(bool state) {
    Z80SetIrqLine(0, state ? Z80_ASSERT_LINE : Z80_CLEAR_LINE);
}

void SoundCPU::nmi() {
    Z80SetIrqLine(Z80_INPUT_LINE_NMI, Z80_ASSERT_LINE);
    Z80SetIrqLine(Z80_INPUT_LINE_NMI, Z80_CLEAR_LINE);
}

void SoundCPU::saveState(Buffer* buf) {
    // Save Z80 state
    Z80_Regs regs;
    Z80GetContext(&regs);
    
    // Calculate size without function pointers
    size_t sizeNoPointers = offsetof(Z80_Regs, irq_callback);
    
    buffer_write(buf, &sizeNoPointers, sizeof(sizeNoPointers));
    buffer_write(buf, &regs, sizeNoPointers);
    buffer_write(buf, &m_cycles, sizeof(m_cycles));
}

void SoundCPU::loadState(Buffer* buf) {
    // Load Z80 state
    size_t sizeNoPointers;
    buffer_read(buf, &sizeNoPointers, sizeof(sizeNoPointers));
    
    if (sizeNoPointers != offsetof(Z80_Regs, irq_callback)) {
        log_error("Error: Saved Z80 context size mismatch");
        return;
    }
    
    // Load saved context (without pointers)
    std::vector<char> savedContext(sizeNoPointers);
    buffer_read(buf, savedContext.data(), sizeNoPointers);
    
    // Get current context
    Z80_Regs currentRegs;
    Z80GetContext(&currentRegs);
    
    // Copy saved context to current context, only copy the non-pointer portion
    memcpy(&currentRegs, savedContext.data(), sizeNoPointers);
    
    // Set current context back
    Z80SetContext(&currentRegs);
    
    buffer_read(buf, &m_cycles, sizeof(m_cycles));
}

} // namespace neogeo
