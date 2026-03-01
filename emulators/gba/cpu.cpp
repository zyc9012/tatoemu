#include "cpu.h"
#include "memory.h"
#include <cstring>

namespace gba {

CPU::CPU() = default;

CPU::~CPU() = default;

void CPU::setMemory(Memory* memory) {
    m_memory = memory;

    MemoryInterface mem{};
    mem.read8  = [](u32 a, void* c) -> u8  { return static_cast<Memory*>(c)->read8(a); };
    mem.read16 = [](u32 a, void* c) -> u16 { return static_cast<Memory*>(c)->read16(a); };
    mem.read32 = [](u32 a, void* c) -> u32 { return static_cast<Memory*>(c)->read32(a); };
    mem.write8  = [](u32 a, u8  d, void* c) { static_cast<Memory*>(c)->write8(a, d); };
    mem.write16 = [](u32 a, u16 d, void* c) { static_cast<Memory*>(c)->write16(a, d); };
    mem.write32 = [](u32 a, u32 d, void* c) { static_cast<Memory*>(c)->write32(a, d); };
    mem.fetch16 = [](u32 a, void* c) -> u16 { return static_cast<Memory*>(c)->fetch16(a); };
    mem.fetch32 = [](u32 a, void* c) -> u32 { return static_cast<Memory*>(c)->fetch32(a); };
    mem.consumeWaitCycles = [](void* c) -> int { return static_cast<Memory*>(c)->consumeWaitCycles(); };
    mem.userData = memory;
    m_arm7.setMemory(mem);
}

void CPU::reset() {
    m_arm7.reset();

    // Boot through BIOS: start at the reset vector
    m_arm7.setReg(R13, SP_USR);       // SP_usr
    m_arm7.setReg(R15, 0x08000000);   // PC = ROM start

    // Set IRQ-mode stack pointer
    m_arm7.setCPSR(0x12 | 0xC0); // IRQ mode
    m_arm7.setReg(R13, SP_IRQ);

    // Set SVC-mode stack pointer
    m_arm7.setCPSR(0x13 | 0xC0); // SVC mode
    m_arm7.setReg(R13, SP_SVC);

    // Switch to System mode for normal execution
    m_arm7.setCPSR(0x1F | 0xC0); // System mode, IRQ/FIQ disabled
    m_arm7.setReg(R13, SP_USR);
}

int CPU::step(int cycles) {
    // Check for pending IRQs BEFORE executing
    checkIRQ();

    if (m_memory->isHalted())
        return 1; // Halted — return minimal cycles

    return m_arm7.execute(cycles);
}

void CPU::checkIRQ() {
    u16 IE  = m_memory->readIO16(IO::IE);
    u16 IF  = m_memory->readIO16(IO::IF);
    u16 IME = m_memory->readIO16(IO::IME);

    // Always wake from halt when any enabled interrupt is pending,
    if (IE & IF) {
        m_memory->setHalted(false);
    }

    // Only assert the IRQ line when IME is also enabled.
    // The ARM7TDMI core checks CPSR.I internally.
    if ((IME & 1) && (IE & IF)) {
        m_arm7.setIRQLine(IRQ_IRQ, 1);
    } else {
        m_arm7.setIRQLine(IRQ_IRQ, 0);
    }
}

void CPU::saveState(Buffer* buf) {
    ARM7TDMI::State st;
    m_arm7.getContext(&st);
    buffer_write(buf, &st, sizeof(st));
    buffer_write(buf, &m_cycles, sizeof(m_cycles));
}

void CPU::loadState(Buffer* buf) {
    ARM7TDMI::State st;
    buffer_read(buf, &st, sizeof(st));
    m_arm7.setContext(&st);
    buffer_read(buf, &m_cycles, sizeof(m_cycles));
}

} // namespace gba
