#include "cpu.h"
#include "memory.h"
#include "../components/cpu/m6502/m6502.h"
#include <cstddef>
#include <cstring>
#include <vector>

namespace {
static nes::CPU* g_nesCpuContext = nullptr;

static nes::Memory* getMemory() {
    if (g_nesCpuContext) {
        return g_nesCpuContext->getMemory();
    }
    return nullptr;
}
}

unsigned char M6502ReadPort(unsigned short address) {
    nes::Memory* memory = getMemory();
    if (memory) {
        return memory->cpuRead(address);
    }
    return 0;
}

void M6502WritePort(unsigned short address, unsigned char data) {
    nes::Memory* memory = getMemory();
    if (memory) {
        memory->cpuWrite(address, data);
    }
}

unsigned char M6502ReadByte(unsigned short address) {
    nes::Memory* memory = getMemory();
    if (memory) {
        return memory->cpuRead(address);
    }
    return 0;
}

void M6502WriteByte(unsigned short address, unsigned char data) {
    nes::Memory* memory = getMemory();
    if (memory) {
        memory->cpuWrite(address, data);
    }
}

unsigned char M6502ReadOp(unsigned short address) {
    return M6502ReadByte(address);
}

unsigned char M6502ReadOpArg(unsigned short address) {
    return M6502ReadByte(address);
}

namespace nes {

CPU::CPU()
    : m_memory(nullptr)
    , m_cycles(0)
    , m_stallCycles(0) {

    n2a03_init();
    g_nesCpuContext = this;
}

CPU::~CPU() {
    if (g_nesCpuContext == this) {
        g_nesCpuContext = nullptr;
    }
}

void CPU::reset() {
    g_nesCpuContext = this;
    m6502_reset();
    m_cycles = 0;
    m_stallCycles = 0;
}

void CPU::step() {
    if (m_stallCycles > 0) {
        m_stallCycles--;
        m_cycles++;
        return;
    }

    g_nesCpuContext = this;
    int executed = m6502_execute(1);
    if (executed < 0) {
        executed = 0;
    }
    m_cycles += static_cast<u32>(executed);
}

void CPU::nmi() {
    m6502_set_nmi_hold2();
}

void CPU::irq() {
    m6502_set_irq_line(M6502_IRQ_LINE, M6502_ASSERT_LINE);
    m6502_set_irq_hold();
}

void CPU::triggerOAMDMA(u8 page) {
    (void)page;

    u32 dmaCycles = 513;
    if (m_cycles & 1) {
        dmaCycles = 514;
    }
    m_stallCycles += dmaCycles;
}

void CPU::saveState(Buffer* buf) {
    m6502_Regs regs = {};
    m6502_get_context(&regs);

    size_t sizeNoPointers = offsetof(m6502_Regs, irq_callback);
    buffer_write(buf, &sizeNoPointers, sizeof(sizeNoPointers));
    buffer_write(buf, &regs, sizeNoPointers);
    buffer_write(buf, &regs.fetching_opcode, sizeof(regs.fetching_opcode));
    buffer_write(buf, &m_cycles, sizeof(m_cycles));
    buffer_write(buf, &m_stallCycles, sizeof(m_stallCycles));
}

void CPU::loadState(Buffer* buf) {
    size_t sizeNoPointers = 0;
    buffer_read(buf, &sizeNoPointers, sizeof(sizeNoPointers));

    if (sizeNoPointers != offsetof(m6502_Regs, irq_callback)) {
        log_error("Error: Saved CPU context size mismatch");
        return;
    }

    std::vector<char> savedContext(sizeNoPointers);
    buffer_read(buf, savedContext.data(), sizeNoPointers);

    int fetchingOpcode = 0;
    buffer_read(buf, &fetchingOpcode, sizeof(fetchingOpcode));

    m6502_Regs current = {};
    m6502_get_context(&current);
    std::memcpy(&current, savedContext.data(), sizeNoPointers);
    current.irq_callback = nullptr;
    current.fetching_opcode = fetchingOpcode;
    m6502_set_context(&current);

    buffer_read(buf, &m_cycles, sizeof(m_cycles));
    buffer_read(buf, &m_stallCycles, sizeof(m_stallCycles));

    g_nesCpuContext = this;
}

} // namespace nes
