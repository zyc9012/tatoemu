#include "cpu.h"
#include "mmu.h"
#include <cstring>

namespace gb {

CPU::CPU()
    : m_mmu(nullptr)
    , m_gbcMode(false) {
    SM83::MemoryInterface mem{};
    mem.read = [](u16 addr, void* ctx) -> u8 {
        MMU* mmu = static_cast<CPU*>(ctx)->getMMU();
        return mmu ? mmu->read(addr) : 0xFF;
    };
    mem.write = [](u16 addr, u8 val, void* ctx) {
        MMU* mmu = static_cast<CPU*>(ctx)->getMMU();
        if (mmu) mmu->write(addr, val);
    };
    mem.stop = [](void* ctx) {
        auto* cpu = static_cast<CPU*>(ctx);
        if (cpu->isGBCMode()) {
            MMU* mmu = cpu->getMMU();
            if (mmu) mmu->performSpeedSwitch();
        }
    };
    mem.userData = this;
    m_sm83.setMemory(mem);
}

CPU::~CPU() = default;

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
    u8 ifReg = m_mmu->read(0xFF0F);
    m_mmu->write(0xFF0F, ifReg | interrupt);
}

void CPU::saveState(Buffer* buf) {
    SM83::State state;
    m_sm83.getContext(&state);
    buffer_write(buf, &state, sizeof(state));
    buffer_write(buf, &m_gbcMode, sizeof(m_gbcMode));
}

void CPU::loadState(Buffer* buf) {
    SM83::State state;
    buffer_read(buf, &state, sizeof(state));
    m_sm83.setContext(&state);
    buffer_read(buf, &m_gbcMode, sizeof(m_gbcMode));
}

} // namespace gb
