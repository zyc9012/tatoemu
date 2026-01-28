#include "cpu.h"
#include "memory.h"
#include <iostream>
#include <vector>
#include "../../components/cpu/m68k/m68k.h"
#include "../../components/cpu/m68k/m68kcb.h"

namespace cps {

// Wrapper functions to convert Memory* methods to C callbacks
static Memory* g_memory = nullptr;

static unsigned int read8_callback(unsigned int address) {
    if (g_memory) {
        return g_memory->read8(address);
    }
    return 0;
}

static unsigned int read16_callback(unsigned int address) {
    if (g_memory) {
        return g_memory->read16(address);
    }
    return 0;
}

static unsigned int read32_callback(unsigned int address) {
    if (g_memory) {
        return g_memory->read32(address);
    }
    return 0;
}

static unsigned int read8Data_callback(unsigned int address) {
    if (g_memory) {
        return g_memory->read8Data(address);
    }
    return 0;
}

static unsigned int read16Data_callback(unsigned int address) {
    if (g_memory) {
        return g_memory->read16Data(address);
    }
    return 0;
}

static unsigned int read32Data_callback(unsigned int address) {
    if (g_memory) {
        return g_memory->read32Data(address);
    }
    return 0;
}

static void write8_callback(unsigned int address, unsigned int value) {
    if (g_memory) {
        g_memory->write8(address, static_cast<u8>(value));
    }
}

static void write16_callback(unsigned int address, unsigned int value) {
    if (g_memory) {
        g_memory->write16(address, static_cast<u16>(value));
    }
}

static void write32_callback(unsigned int address, unsigned int value) {
    if (g_memory) {
        g_memory->write32(address, static_cast<u32>(value));
    }
}

static void write32_pd_callback(unsigned int address, unsigned int value) {
    // Predecrement mode - write high word first, then low word
    if (g_memory) {
        g_memory->write16(address + 2, static_cast<u16>((value >> 16) & 0xFFFF));
        g_memory->write16(address, static_cast<u16>(value & 0xFFFF));
    }
}

CPU::CPU()
    : m_memory(nullptr) {
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
    return m68k_execute(cycles);
}

void CPU::irq(u8 level) {
    // Set interrupt level (0-7, where 0 = no interrupt, 7 = NMI)
    if (level > 7) level = 7;
    m68k_set_irq(level);
}

void CPU::saveState(std::ofstream& file) {
    // Save Musashi CPU context
    unsigned int contextSize = m68k_context_size();
    unsigned int contextSizeNoPointers = m68k_context_size_no_pointers();
    std::vector<char> context(contextSize);
    m68k_get_context(context.data());
    
    file.write(reinterpret_cast<const char*>(&contextSizeNoPointers), sizeof(contextSizeNoPointers));
    file.write(context.data(), contextSizeNoPointers);
}

void CPU::loadState(std::ifstream& file) {
    // Load Musashi CPU context
    unsigned int contextSizeNoPointers;
    unsigned int contextSize = m68k_context_size();
    file.read(reinterpret_cast<char*>(&contextSizeNoPointers), sizeof(contextSizeNoPointers));

    if (contextSizeNoPointers != m68k_context_size_no_pointers()) {
        std::cerr << "Error: Saved CPU context size mismatch" << std::endl;
        return;
    }
    
    // Load saved context (without pointers)
    std::vector<char> savedContext(contextSizeNoPointers);
    file.read(savedContext.data(), contextSizeNoPointers);

    // Get current context
    std::vector<char> currentContext(contextSize);
    m68k_get_context(currentContext.data());

    // Copy saved context to current context, only copy the non-pointer portion
    memcpy(currentContext.data(), savedContext.data(), contextSizeNoPointers);

    // Set current context back
    m68k_set_context(currentContext.data());
    
    // Ensure memory pointer is set
    g_memory = m_memory;
}

} // namespace cps
