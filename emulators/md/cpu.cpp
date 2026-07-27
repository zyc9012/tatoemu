#include "cpu.h"
#include "memory.h"
#include <algorithm>
#include <cstring>
#include <vector>

#include "../../components/cpu/m68k/m68k.h"
#include "../../components/cpu/m68k/m68kcb.h"

namespace md {

static Memory* g_memory = nullptr;

static unsigned int read8_callback(unsigned int address) {
    return g_memory->read8(address & 0xFFFFFF);
}

static unsigned int read16_callback(unsigned int address) {
    return g_memory->read16(address & 0xFFFFFE);
}

static unsigned int read32_callback(unsigned int address) {
    address &= 0xFFFFFE;
    return (static_cast<unsigned int>(g_memory->read16(address)) << 16) |
            static_cast<unsigned int>(g_memory->read16(address + 2));
}

static void write8_callback(unsigned int address, unsigned int value) {
    g_memory->write8(address & 0xFFFFFF, static_cast<u8>(value));
}

static void write16_callback(unsigned int address, unsigned int value) {
    g_memory->write16(address & 0xFFFFFE, static_cast<u16>(value));
}

static void write32_callback(unsigned int address, unsigned int value) {
    address &= 0xFFFFFE;
    g_memory->write16(address, static_cast<u16>(value >> 16));
    g_memory->write16(address + 2, static_cast<u16>(value & 0xFFFF));
}

static void write32_pd_callback(unsigned int address, unsigned int value) {
    address &= 0xFFFFFE;
    g_memory->write16(address + 2, static_cast<u16>(value >> 16));
    g_memory->write16(address, static_cast<u16>(value & 0xFFFF));
}

CPU::CPU() {
    static bool initialized = false;
    if (!initialized) {
        m68k_init();
        initialized = true;
    }
    m68k_set_cpu_type(M68K_CPU_TYPE_68000);
}

CPU::~CPU() = default;

void CPU::reset() {
    g_memory = m_memory;

    m68k_memory_callbacks callbacks = {};
    callbacks.read_memory_8 = read8_callback;
    callbacks.read_memory_16 = read16_callback;
    callbacks.read_memory_32 = read32_callback;
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

    m68k_pulse_reset();

    m_cycles = 0;
    m_stolenCycles = 0;
    m_pendingStall = 0;
    m_executing = false;
}

u32 CPU::frameCycles() const {
    return m_executing ? m_cycles + static_cast<u32>(m68k_cycles_run()) + m_stolenCycles
                       : m_cycles;
}

u32 CPU::step(u32 cycles) {
    if (cycles == 0) return 0;

    g_memory = m_memory;

    u32 consumed = 0;

    // Pay off any DMA stall left over from a previous slice first.
    if (m_pendingStall > 0) {
        const u32 take = std::min(m_pendingStall, cycles);
        m_pendingStall -= take;
        consumed += take;
        cycles -= take;
    }

    if (cycles == 0) {
        m_cycles += consumed;
        return consumed;
    }

    m_stolenCycles = 0;
    m_executing = true;
    const u32 used = static_cast<u32>(m68k_execute(static_cast<int>(cycles)));
    m_executing = false;

    consumed += used + m_stolenCycles;
    m_stolenCycles = 0;
    m_cycles += consumed;

    return consumed;
}

void CPU::setIRQLevel(u8 level) {
    if (level > 7) level = 7;
    m68k_set_irq(level);
}

void CPU::stall(u32 cycles) {
    if (cycles == 0) return;

    if (m_executing) {
        const int remaining = m68k_cycles_remaining();
        int steal = static_cast<int>(std::min<u32>(cycles, static_cast<u32>(std::max(remaining, 0))));
        if (steal > 0) {
            m68k_modify_timeslice(-steal);
            m_stolenCycles += static_cast<u32>(steal);
            cycles -= static_cast<u32>(steal);
        }
    }

    m_pendingStall += cycles;
}

void CPU::saveState(Buffer* buf) {
    unsigned int contextSize = m68k_context_size();
    unsigned int contextSizeNoPointers = m68k_context_size_no_pointers();
    std::vector<char> context(contextSize);
    m68k_get_context(context.data());

    buffer_write(buf, &contextSizeNoPointers, sizeof(contextSizeNoPointers));
    buffer_write(buf, context.data(), contextSizeNoPointers);
    buffer_write(buf, &m_cycles, sizeof(m_cycles));
}

void CPU::loadState(Buffer* buf) {
    unsigned int contextSizeNoPointers = 0;
    unsigned int contextSize = m68k_context_size();
    buffer_read(buf, &contextSizeNoPointers, sizeof(contextSizeNoPointers));

    if (contextSizeNoPointers != m68k_context_size_no_pointers()) {
        log_error("Error: Saved 68000 context size mismatch");
        return;
    }

    std::vector<char> savedContext(contextSizeNoPointers);
    buffer_read(buf, savedContext.data(), contextSizeNoPointers);

    std::vector<char> currentContext(contextSize);
    m68k_get_context(currentContext.data());
    memcpy(currentContext.data(), savedContext.data(), contextSizeNoPointers);
    m68k_set_context(currentContext.data());

    buffer_read(buf, &m_cycles, sizeof(m_cycles));

    g_memory = m_memory;
}

} // namespace md
