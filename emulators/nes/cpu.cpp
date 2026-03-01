#include "cpu.h"
#include "memory.h"
#include "../components/cpu/m6502/m6502.h"
#include <cstddef>
#include <cstring>

namespace nes {

CPU::CPU()
    : m_memory(nullptr)
    , m_m6502(std::make_unique<M6502>(M6502::Variant::NMOS_2A03))
    , m_cycles(0)
    , m_stallCycles(0) {
}

CPU::~CPU() = default;

void CPU::setMemory(Memory* memory) {
    m_memory = memory;

    M6502::MemoryInterface mem{};
    mem.read = [](uint16_t addr, void* ctx) -> uint8_t {
        return static_cast<Memory*>(ctx)->cpuRead(addr);
    };
    mem.write = [](uint16_t addr, uint8_t val, void* ctx) {
        static_cast<Memory*>(ctx)->cpuWrite(addr, val);
    };
    mem.userData = memory;
    m_m6502->setMemory(mem);
}

/**
 * @brief Reset the CPU
 * 
 * Resets the 2A03 CPU to initial state and clears cycle counters.
 */
void CPU::reset() {
    m_m6502->reset();
    m_cycles = 0;
    m_stallCycles = 0;
}

/**
 * @brief Execute CPU for specified number of cycles
 * @param cycles Number of cycles to execute
 * 
 * Handles DMA stalling and executes CPU instructions.
 */
void CPU::step(u32 cycles) {
    // Handle DMA stalls
    if (m_stallCycles > 0) {
        m_stallCycles--;
        m_cycles++;
        return;
    }

    // Execute CPU
    int executed = m_m6502->execute(static_cast<int>(cycles));
    m_cycles += static_cast<u32>(executed);
}

/**
 * @brief Trigger NMI (Non-Maskable Interrupt)
 * 
 * NES-specific: NMI is delayed by 2 cycles when triggered during VBLANK.
 */
void CPU::nmi() {
    m_m6502->setNMIDelay(2);
    m_m6502->setHoldNMI(true);
}

/**
 * @brief Set IRQ (Interrupt Request) line state
 * @param state 1 = assert, 0 = clear
 */
void CPU::irq(u32 state) {
    m_m6502->setInterruptLine(
        M6502::InterruptLine::IRQ,
        state ? M6502::LineState::ASSERT 
              : M6502::LineState::CLEAR
    );
}

/**
 * @brief Trigger OAM DMA transfer
 * @param page High byte of source address
 * 
 * OAM DMA takes 513 cycles, or 514 if starting on an odd cycle.
 */
void CPU::triggerOAMDMA(u8 page) {
    (void)page;

    u32 dmaCycles = 513;
    if (m_cycles & 1) {
        dmaCycles = 514;
    }
    m_stallCycles += dmaCycles;
}

/**
 * @brief Save CPU state to buffer
 * @param buf Buffer to write state to
 */
void CPU::saveState(Buffer* buf) {
    M6502::State state;
    m_m6502->getContext(&state);
    
    buffer_write(buf, &state, sizeof(state));
    buffer_write(buf, &m_cycles, sizeof(m_cycles));
    buffer_write(buf, &m_stallCycles, sizeof(m_stallCycles));
}

/**
 * @brief Load CPU state from buffer
 * @param buf Buffer to read state from
 */
void CPU::loadState(Buffer* buf) {
    M6502::State state;
    buffer_read(buf, &state, sizeof(state));
    m_m6502->setContext(&state);
    
    buffer_read(buf, &m_cycles, sizeof(m_cycles));
    buffer_read(buf, &m_stallCycles, sizeof(m_stallCycles));
}

} // namespace nes
