#include "cpu.h"
#include "memory.h"
#include "../components/cpu/arm7/arm7_intf.h"
#include "../components/cpu/arm7/arm7core.h"
#include <cstring>

namespace gba {

// Global pointer to current CPU (for ARM7 callbacks)
static CPU* g_currentCPU = nullptr;

// ARM7 memory access callbacks
extern "C" {

UINT8 Arm7ReadByte(UINT32 addr) {
    if (g_currentCPU && g_currentCPU->m_memory) {
        return g_currentCPU->m_memory->read8(addr);
    }
    return 0xFF;
}

UINT16 Arm7ReadWord(UINT32 addr) {
    if (g_currentCPU && g_currentCPU->m_memory) {
        return g_currentCPU->m_memory->read16(addr);
    }
    return 0xFFFF;
}

UINT32 Arm7ReadLong(UINT32 addr) {
    if (g_currentCPU && g_currentCPU->m_memory) {
        return g_currentCPU->m_memory->read32(addr);
    }
    return 0xFFFFFFFF;
}

void Arm7WriteByte(UINT32 addr, UINT8 data) {
    if (g_currentCPU && g_currentCPU->m_memory) {
        g_currentCPU->m_memory->write8(addr, data);
    }
}

void Arm7WriteWord(UINT32 addr, UINT16 data) {
    if (g_currentCPU && g_currentCPU->m_memory) {
        g_currentCPU->m_memory->write16(addr, data);
    }
}

void Arm7WriteLong(UINT32 addr, UINT32 data) {
    if (g_currentCPU && g_currentCPU->m_memory) {
        g_currentCPU->m_memory->write32(addr, data);
    }
}

UINT16 Arm7FetchWord(UINT32 addr) {
    if (g_currentCPU && g_currentCPU->m_memory) {
        return g_currentCPU->m_memory->fetch16(addr);
    }
    return 0xFFFF;
}

UINT32 Arm7FetchLong(UINT32 addr) {
    if (g_currentCPU && g_currentCPU->m_memory) {
        return g_currentCPU->m_memory->fetch32(addr);
    }
    return 0xFFFFFFFF;
}

} // extern "C"

CPU::CPU() {
    g_currentCPU = this;
}

CPU::~CPU() {
    if (g_currentCPU == this) {
        g_currentCPU = nullptr;
    }
}

void CPU::reset() {
    Arm7Reset();
    
    if (m_memory && m_memory->hasBIOS()) {
        // Boot through BIOS: start at 0x00000000
        Arm7SetRegister(15, 0x00000000);
        // BIOS initializes stack pointers and mode
        Arm7SetCPSR(0x13 | 0xC0); // SVC mode, IRQ/FIQ disabled
    } else {
        // Skip BIOS: set up post-boot state
        Arm7SetRegister(13, SP_USR); // SP_usr
        Arm7SetRegister(15, 0x08000000); // PC = ROM start
        
        // Set IRQ mode SP
        Arm7SetCPSR(0x12 | 0xC0); // IRQ mode
        Arm7SetRegister(13, SP_IRQ);
        
        // Set SVC mode SP
        Arm7SetCPSR(0x13 | 0xC0); // SVC mode
        Arm7SetRegister(13, SP_SVC);
        
        // Switch to System mode
        Arm7SetCPSR(0x1F | 0xC0); // System mode, IRQ/FIQ disabled
        Arm7SetRegister(13, SP_USR); // SP_usr again in system mode
    }
    
    Arm7NewFrame();
}

int CPU::step() {
    if (m_memory && m_memory->isHalted()) {
        return 1; // Halted, return minimal cycles
    }
    
    int cycles = Arm7Run(1);
    checkIRQ();
    return cycles;
}

void CPU::raiseIRQ() {
    arm7_set_irq_line(ARM7_IRQ_LINE, 1);
}

void CPU::checkIRQ() {
    if (!m_memory) return;
    
    u16 IE = m_memory->readIO16(IO::IE);
    u16 IF = m_memory->readIO16(IO::IF);
    u16 IME = m_memory->readIO16(IO::IME);
    
    if ((IME & 1) && (IE & IF)) {
        m_memory->setHalted(false);
        raiseIRQ();
    } else {
        arm7_set_irq_line(ARM7_IRQ_LINE, 0);
    }
}

void CPU::saveState(Buffer* buf) {
    void* state = Arm7GetState();
    int size = Arm7GetStateSize();
    buffer_write(buf, state, size);
    buffer_write(buf, &m_cycles, sizeof(m_cycles));
}

void CPU::loadState(Buffer* buf) {
    void* state = Arm7GetState();
    int size = Arm7GetStateSize();
    buffer_read(buf, state, size);
    buffer_read(buf, &m_cycles, sizeof(m_cycles));
}

} // namespace gba
