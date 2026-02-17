#include "cpu.h"
#include "memory.h"
#include <cstring>

namespace gba {

// ---------------------------------------------------------------------------
// File-local memory pointer so plain function pointers can reach Memory.
// Only one CPU instance is expected — this mirrors the old g_currentCPU pattern.
// ---------------------------------------------------------------------------
static Memory* s_memory = nullptr;

// Memory callback trampolines (ARM7TDMI MemoryInterface uses plain fn ptrs)
static u8  memRead8 (u32 a) { return s_memory->read8(a);  }
static u16 memRead16(u32 a) { return s_memory->read16(a); }
static u32 memRead32(u32 a) { return s_memory->read32(a); }
static void memWrite8 (u32 a, u8  d) { s_memory->write8(a, d);  }
static void memWrite16(u32 a, u16 d) { s_memory->write16(a, d); }
static void memWrite32(u32 a, u32 d) { s_memory->write32(a, d); }
static u16 memFetch16(u32 a) { return s_memory->fetch16(a); }
static u32 memFetch32(u32 a) { return s_memory->fetch32(a); }

CPU::CPU() = default;

CPU::~CPU() {
    if (s_memory == m_memory)
        s_memory = nullptr;
}

void CPU::setMemory(Memory* memory) {
    m_memory = memory;
    s_memory = memory;

    MemoryInterface mem{};
    mem.read8   = memRead8;
    mem.read16  = memRead16;
    mem.read32  = memRead32;
    mem.write8  = memWrite8;
    mem.write16 = memWrite16;
    mem.write32 = memWrite32;
    mem.fetch16 = memFetch16;
    mem.fetch32 = memFetch32;
    m_cpu.setMemory(mem);
}

void CPU::reset() {
    m_cpu.reset();

    // Boot through BIOS: start at the reset vector
    m_cpu.setReg(R15, 0x00000000);
    m_cpu.setCPSR(0x13 | 0xC0); // SVC mode, IRQ/FIQ disabled
}

int CPU::step(int cycles) {
    if (m_memory && m_memory->isHalted())
        return 1; // Halted — return minimal cycles

    int executed = m_cpu.run(cycles);
    // Add memory wait state cycles accumulated during this step
    if (m_memory) executed += m_memory->consumeWaitCycles();
    checkIRQ();
    return executed;
}

void CPU::raiseIRQ() {
    m_cpu.setIRQLine(IRQ_IRQ, 1);
}

void CPU::checkIRQ() {
    if (!m_memory) return;

    u16 IE  = m_memory->readIO16(IO::IE);
    u16 IF  = m_memory->readIO16(IO::IF);
    u16 IME = m_memory->readIO16(IO::IME);

    if ((IME & 1) && (IE & IF)) {
        m_memory->setHalted(false);
        raiseIRQ();
    } else {
        m_cpu.setIRQLine(IRQ_IRQ, 0);
    }
}

void CPU::saveState(Buffer* buf) {
    auto st = m_cpu.saveState();
    buffer_write(buf, &st, sizeof(st));
    buffer_write(buf, &m_cycles, sizeof(m_cycles));
}

void CPU::loadState(Buffer* buf) {
    ARM7TDMI::State st;
    buffer_read(buf, &st, sizeof(st));
    m_cpu.loadState(st);
    buffer_read(buf, &m_cycles, sizeof(m_cycles));
}

} // namespace gba
