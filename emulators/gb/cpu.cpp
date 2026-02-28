#include "cpu.h"
#include "mmu.h"
#include <cstring>

namespace gb {

// ---------------------------------------------------------------------------
// Static callback context — the SM83 core uses plain function pointers, so
// we need a global to route back to the owning CPU/MMU.  (Same pattern used
// by the Z80 wrapper in neogeo::SoundCPU.)
// ---------------------------------------------------------------------------
static CPU* g_cpuCtx = nullptr;

static u8 sm83Read(u16 address) {
    if (g_cpuCtx) {
        MMU* mmu = g_cpuCtx->getMMU();
        if (mmu) return mmu->read(address);
    }
    return 0xFF;
}

static void sm83Write(u16 address, u8 value) {
    if (g_cpuCtx) {
        MMU* mmu = g_cpuCtx->getMMU();
        if (mmu) mmu->write(address, value);
    }
}

// ---------------------------------------------------------------------------

CPU::CPU()
    : m_mmu(nullptr)
    , m_gbcMode(false) {
    g_cpuCtx = this;
    m_sm83.setReadHandler(sm83Read);
    m_sm83.setWriteHandler(sm83Write);
}

CPU::~CPU() {
    if (g_cpuCtx == this) g_cpuCtx = nullptr;
}

void CPU::setMMU(MMU* mmu) {
    m_mmu = mmu;
}

void CPU::reset(bool useBootrom) {
    m_sm83.reset();

    if (useBootrom) {
        m_sm83.setAF(0x0000);
        m_sm83.setBC(0x0000);
        m_sm83.setDE(0x0000);
        m_sm83.setHL(0x0000);
        m_sm83.setSP(0x0000);
        m_sm83.setPC(0x0000);
    } else {
        if (m_gbcMode) {
            m_sm83.setAF(0x1180);
            m_sm83.setBC(0x0000);
            m_sm83.setDE(0xFF56);
            m_sm83.setHL(0x000D);
        } else {
            m_sm83.setAF(0x01B0);
            m_sm83.setBC(0x0013);
            m_sm83.setDE(0x00D8);
            m_sm83.setHL(0x014D);
        }
        m_sm83.setSP(0xFFFE);
        m_sm83.setPC(0x0100);
    }
}

u32 CPU::step() {
    // Execute exactly one instruction's worth of cycles via the SM83 core.
    // We ask for 1 cycle; execute() will run at least one instruction and
    // return the actual number of T-cycles consumed.
    return static_cast<u32>(m_sm83.execute(1));
}

void CPU::requestInterrupt(u8 interrupt) {
    // Set the corresponding bit in the IF register (0xFF0F) via the bus.
    if (m_mmu) {
        u8 ifReg = m_mmu->read(0xFF0F);
        m_mmu->write(0xFF0F, ifReg | interrupt);
    }
}

void CPU::saveState(Buffer* buf) {
    SM83::Regs regs;
    m_sm83.getContext(&regs);
    buffer_write(buf, &regs, sizeof(regs));
    buffer_write(buf, &m_gbcMode, sizeof(m_gbcMode));
}

void CPU::loadState(Buffer* buf) {
    SM83::Regs regs;
    buffer_read(buf, &regs, sizeof(regs));
    m_sm83.setContext(&regs);
    buffer_read(buf, &m_gbcMode, sizeof(m_gbcMode));
}

} // namespace gb
