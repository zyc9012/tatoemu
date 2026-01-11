#include "cpu.h"
#include "memory_base.h"
#include "m68k/m68k.h"
#include <iostream>
#include <cstring>
#include <iomanip>
#include <cstdio>
#include <vector>

namespace cps {

// Global pointer to memory interface for Musashi callbacks
static MemoryBase* g_memory = nullptr;

// Musashi memory access callbacks
extern "C" unsigned int m68k_read_memory_8(unsigned int address) {
    if (g_memory) {
        return g_memory->read8(address);
    }
    return 0;
}

extern "C" unsigned int m68k_read_memory_16(unsigned int address) {
    if (g_memory) {
        return g_memory->read16(address);
    }
    return 0;
}

extern "C" unsigned int m68k_read_memory_32(unsigned int address) {
    if (g_memory) {
        return g_memory->read32(address);
    }
    return 0;
}

extern "C" unsigned int m68k_read_immediate_16(unsigned int address) {
    if (g_memory) {
        return g_memory->read16(address);
    }
    return 0;
}

extern "C" unsigned int m68k_read_immediate_32(unsigned int address) {
    if (g_memory) {
        return g_memory->read32(address);
    }
    return 0;
}

extern "C" unsigned int m68k_read_pcrelative_8(unsigned int address) {
    if (g_memory) {
        return g_memory->read8(address);
    }
    return 0;
}

extern "C" unsigned int m68k_read_pcrelative_16(unsigned int address) {
    if (g_memory) {
        return g_memory->read16(address);
    }
    return 0;
}

extern "C" unsigned int m68k_read_pcrelative_32(unsigned int address) {
    if (g_memory) {
        return g_memory->read32(address);
    }
    return 0;
}

extern "C" unsigned int m68k_read_disassembler_8(unsigned int address) {
    if (g_memory) {
        return g_memory->read8(address);
    }
    return 0;
}

extern "C" unsigned int m68k_read_disassembler_16(unsigned int address) {
    if (g_memory) {
        return g_memory->read16(address);
    }
    return 0;
}

extern "C" unsigned int m68k_read_disassembler_32(unsigned int address) {
    if (g_memory) {
        return g_memory->read32(address);
    }
    return 0;
}

extern "C" void m68k_write_memory_8(unsigned int address, unsigned int value) {
    if (g_memory) {
        g_memory->write8(address, static_cast<u8>(value));
    }
}

extern "C" void m68k_write_memory_16(unsigned int address, unsigned int value) {
    if (g_memory) {
        g_memory->write16(address, static_cast<u16>(value));
    }
}

extern "C" void m68k_write_memory_32(unsigned int address, unsigned int value) {
    if (g_memory) {
        g_memory->write32(address, static_cast<u32>(value));
    }
}

extern "C" void m68k_write_memory_32_pd(unsigned int address, unsigned int value) {
    // Predecrement mode - write high word first, then low word
    if (g_memory) {
        g_memory->write16(address + 2, static_cast<u16>((value >> 16) & 0xFFFF));
        g_memory->write16(address, static_cast<u16>(value & 0xFFFF));
    }
}

CPU::CPU()
    : m_memory(nullptr)
    , m_cycles(0) {
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
    
    // Reset the Musashi emulator
    m68k_pulse_reset();
    
    // Reset cycle counter
    m_cycles = 0;
}

void CPU::step() {
    // Set global memory pointer for callbacks
    g_memory = m_memory;
    
    // Execute cycles - Musashi executes by cycles, not by instruction
    // Most 68000 instructions take 4-20 cycles. We execute 10 cycles which should
    // typically result in one instruction per call. Occasionally we may execute
    // multiple very short instructions (e.g., two 4-cycle instructions), but this
    // is acceptable and maintains cycle accuracy for the emulator.
    int cyclesUsed = m68k_execute(10);
    m_cycles += cyclesUsed;
}

// getCycles() and setMemory() are defined inline in cpu.h

void CPU::irq(u8 level) {
    // Set interrupt level (0-7, where 0 = no interrupt, 7 = NMI)
    if (level > 7) level = 7;
    m68k_set_irq(level);
}

void CPU::resetInterrupt() {
    // Clear interrupt by setting level to 0
    m68k_set_irq(0);
}

void CPU::saveState(std::ofstream& file) {
    // Save Musashi CPU context
    unsigned int contextSize = m68k_context_size();
    unsigned int contextSizeNoPointers = m68k_context_size_no_pointers();
    std::vector<char> context(contextSize);
    m68k_get_context(context.data());
    
    file.write(reinterpret_cast<const char*>(&contextSizeNoPointers), sizeof(contextSizeNoPointers));
    file.write(context.data(), contextSizeNoPointers);
    file.write(reinterpret_cast<const char*>(&m_cycles), sizeof(m_cycles));
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
    
    file.read(reinterpret_cast<char*>(&m_cycles), sizeof(m_cycles));
    
    // Ensure memory pointer is set
    g_memory = m_memory;
}

} // namespace cps
