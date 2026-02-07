#include "cpu.h"
#include "memory.h"
#include "cartridge.h"
#include "consts.h"
#include <vector>
#include "../../components/cpu/m68k/m68k.h"
#include "../../components/cpu/m68k/m68kcb.h"

namespace cps {

// Wrapper functions to convert Memory* methods to C callbacks
static Memory* g_memory = nullptr;

static unsigned int read8_callback(unsigned int address) {
    if (g_memory) {
        return g_memory->read8(address & 0xFFFFFF);
    }
    return 0;
}

static unsigned int read16_callback(unsigned int address) {
    if (g_memory) {
        return g_memory->read16(address & 0xFFFFFF);
    }
    return 0;
}

static unsigned int read32_callback(unsigned int address) {
    if (g_memory) {
        return g_memory->read32(address & 0xFFFFFF);
    }
    return 0;
}

static unsigned int read8Data_callback(unsigned int address) {
    if (g_memory) {
        return g_memory->read8Data(address & 0xFFFFFF);
    }
    return 0;
}

static unsigned int read16Data_callback(unsigned int address) {
    if (g_memory) {
        return g_memory->read16Data(address & 0xFFFFFF);
    }
    return 0;
}

static unsigned int read32Data_callback(unsigned int address) {
    if (g_memory) {
        return g_memory->read32Data(address & 0xFFFFFF);
    }
    return 0;
}

static void write8_callback(unsigned int address, unsigned int value) {
    if (g_memory) {
        g_memory->write8(address & 0xFFFFFF, static_cast<u8>(value));
    }
}

static void write16_callback(unsigned int address, unsigned int value) {
    if (g_memory) {
        g_memory->write16(address & 0xFFFFFF, static_cast<u16>(value));
    }
}

static void write32_callback(unsigned int address, unsigned int value) {
    if (g_memory) {
        g_memory->write32(address & 0xFFFFFF, static_cast<u32>(value));
    }
}

static void write32_pd_callback(unsigned int address, unsigned int value) {
    // Predecrement mode - write high word first, then low word
    if (g_memory) {
        g_memory->write16((address + 2) & 0xFFFFFF, static_cast<u16>((value >> 16) & 0xFFFF));
        g_memory->write16(address & 0xFFFFFF, static_cast<u16>(value & 0xFFFF));
    }
}

CPU::CPU()
    : m_memory(nullptr)
    , m_cartridge(nullptr)
    , m_cycles(0)
    , m_cyclesPerFrame(0)
    , m_executing(false) {
    // Initialize Musashi emulator (safe to call multiple times)
    static bool initialized = false;
    if (!initialized) {
        m68k_init();
        initialized = true;
    }
    m68k_set_cpu_type(M68K_CPU_TYPE_68000);
}

CPU::~CPU() {
    // Cleanup if needed
}

void CPU::reset() {
    // Set global memory pointer for callbacks
    g_memory = m_memory;

    m_cycles = 0;
    m_executing = false;

    if (m_cartridge->getCPSVersion() == 2) {
        m_cyclesPerFrame = ::cps2::CPU_CYCLES_PER_FRAME;
    } else if (m_cartridge->isCPS1QSound()) {
        m_cyclesPerFrame = ::cps1qs::CPU_CYCLES_PER_FRAME;
    } else {
        m_cyclesPerFrame = ::cps1::CPU_CYCLES_PER_FRAME;
    }
    
    // Set up callbacks - 1:1 match with Musashi interface
    m68k_memory_callbacks callbacks = {};
    callbacks.read_memory_8 = read8Data_callback;
    callbacks.read_memory_16 = read16Data_callback;
    callbacks.read_memory_32 = read32Data_callback;
    callbacks.read_immediate_16 = read16_callback;
    callbacks.read_immediate_32 = read32_callback;
    callbacks.read_pcrelative_8 = read8_callback;
    callbacks.read_pcrelative_16 = read16_callback;
    callbacks.read_pcrelative_32 = read32_callback;
    callbacks.read_disassembler_8 = read8_callback;
    callbacks.read_disassembler_16 = read16_callback;
    callbacks.read_disassembler_32 = read32_callback;
    callbacks.write_memory_8 = write8_callback;
    callbacks.write_memory_16 = write16_callback;
    callbacks.write_memory_32 = write32_callback;
    callbacks.write_memory_32_pd = write32_pd_callback;
    m68k_set_memory_callbacks(&callbacks);
    
    // Reset the Musashi emulator
    m68k_pulse_reset();
}

u32 CPU::step(u32 cycles) {
    // Set global memory pointer for callbacks
    g_memory = m_memory;
    
    // Execute cycles
    m_executing = true;
    u32 cyclesUsed = m68k_execute(cycles);
    m_cycles += cyclesUsed;
    m_executing = false;

    return cyclesUsed;
}

void CPU::irq(u8 level) {
    // Set interrupt level (0-7, where 0 = no interrupt, 7 = NMI)
    if (level > 7) level = 7;
    m68k_set_irq(level);
}

void CPU::saveState(Buffer* buf) {
    // Save Musashi CPU context
    unsigned int contextSize = m68k_context_size();
    unsigned int contextSizeNoPointers = m68k_context_size_no_pointers();
    std::vector<char> context(contextSize);
    m68k_get_context(context.data());
    
    buffer_write(buf, &contextSizeNoPointers, sizeof(contextSizeNoPointers));
    buffer_write(buf, context.data(), contextSizeNoPointers);
    buffer_write(buf, &m_cycles, sizeof(m_cycles));
    buffer_write(buf, &m_cyclesPerFrame, sizeof(m_cyclesPerFrame));
    buffer_write(buf, &m_executing, sizeof(m_executing));
}

void CPU::loadState(Buffer* buf) {
    // Load Musashi CPU context
    unsigned int contextSizeNoPointers;
    unsigned int contextSize = m68k_context_size();
    buffer_read(buf, &contextSizeNoPointers, sizeof(contextSizeNoPointers));

    if (contextSizeNoPointers != m68k_context_size_no_pointers()) {
        log_error("Error: Saved CPU context size mismatch");
        return;
    }
    
    // Load saved context (without pointers)
    std::vector<char> savedContext(contextSizeNoPointers);
    buffer_read(buf, savedContext.data(), contextSizeNoPointers);

    // Get current context
    std::vector<char> currentContext(contextSize);
    m68k_get_context(currentContext.data());

    // Copy saved context to current context, only copy the non-pointer portion
    memcpy(currentContext.data(), savedContext.data(), contextSizeNoPointers);

    // Set current context back
    m68k_set_context(currentContext.data());
    
    buffer_read(buf, &m_cycles, sizeof(m_cycles));
    buffer_read(buf, &m_cyclesPerFrame, sizeof(m_cyclesPerFrame));
    buffer_read(buf, &m_executing, sizeof(m_executing));
    
    // Ensure memory pointer is set
    g_memory = m_memory;
}

} // namespace cps
