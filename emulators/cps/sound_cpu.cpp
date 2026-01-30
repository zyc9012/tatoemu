#include "sound_cpu.h"
#include "memory.h"
#include "apu.h"
#include "consts.h"
#include "../components/cpu/z80/z80.h"
#include <iostream>
#include <cstring>
#include <vector>
#include <cstddef>

namespace cps {

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
        APU* apu = g_soundCpuContext->getAPU();
        if (apu) {
            return apu->readPort(port);
        }
    }
    return 0xFF;
}

static void z80_write_io(u32 port, u8 value) {
    if (g_soundCpuContext) {
        APU* apu = g_soundCpuContext->getAPU();
        if (apu) {
            apu->writePort(port, value);
        }
    }
}

static u8 z80_read_op(u32 address) {
    if (g_soundCpuContext) {
        Memory* mem = g_soundCpuContext->getMemory();
        if (mem) {
            return mem->readZ80Opcode(address);
        }
    }
    return 0xFF;
}

static u8 z80_read_op_arg(u32 address) {
    if (g_soundCpuContext) {
        Memory* mem = g_soundCpuContext->getMemory();
        if (mem) {
            return mem->readZ80OpcodeArg(address);
        }
    }
    return 0xFF;
}

SoundCPU::SoundCPU()
    : m_memory(nullptr)
    , m_apu(nullptr)
    , m_cycles(0)
    , m_cartridge(nullptr)
    , m_cpsVersion(1)
    , m_timerAccumulator(0)
    , m_timerPeriod(0) {
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
    m_timerAccumulator = 0;
    
    // Calculate timer period for CPS2 (252 Hz interrupt rate)
    if (m_cpsVersion == 2 || (m_cpsVersion == 1 && m_cartridge && m_cartridge->isCPS1QSound())) {
        // Timer fires at 252 Hz = 1/252.0 seconds
        // At 8 MHz: 8000000 / 252 ≈ 31746 cycles
        if (m_cartridge->isCPS1QSound()) {
            m_timerPeriod = cps1qs::SOUND_CPU_FREQUENCY / 252;
        } else {
            m_timerPeriod = cps2::SOUND_CPU_FREQUENCY / 252;
        }
    } else {
        m_timerPeriod = 0;  // CPS1 uses YM2151 interrupts, not timer
    }
}

void SoundCPU::setCPSVersion(u8 version) {
    m_cpsVersion = version;
}

u32 SoundCPU::step(u32 cycles) {
    if (cycles > 0) {
        s32 executed = Z80Execute(static_cast<s32>(cycles));
        u32 actualCycles = static_cast<u32>(executed);
        m_cycles += actualCycles;

        // For CPS2 and CPS1 QSound, check timer-based interrupt
        if (m_timerPeriod > 0) {
            m_timerAccumulator += executed;
            
            // Check if timer has expired (multiple times if needed)
            while (m_timerAccumulator >= m_timerPeriod) {
                m_timerAccumulator -= m_timerPeriod;
                // Trigger Z80 IRQ (timer interrupt at 252 Hz)
                ActiveZ80SetIRQHold();
                Z80SetIrqLine(0xFF, Z80_ASSERT_LINE);
            }
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
    buffer_write(buf, &m_cpsVersion, sizeof(m_cpsVersion));
    buffer_write(buf, &m_timerAccumulator, sizeof(m_timerAccumulator));
    buffer_write(buf, &m_timerPeriod, sizeof(m_timerPeriod));
}

void SoundCPU::loadState(Buffer* buf) {
    // Load Z80 state
    size_t sizeNoPointers;
    buffer_read(buf, &sizeNoPointers, sizeof(sizeNoPointers));
    
    if (sizeNoPointers != offsetof(Z80_Regs, irq_callback)) {
        std::cerr << "Error: Saved Z80 context size mismatch" << std::endl;
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
    buffer_read(buf, &m_cpsVersion, sizeof(m_cpsVersion));
    buffer_read(buf, &m_timerAccumulator, sizeof(m_timerAccumulator));
    buffer_read(buf, &m_timerPeriod, sizeof(m_timerPeriod));
}

} // namespace cps
