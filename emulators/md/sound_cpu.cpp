#include "sound_cpu.h"
#include "memory.h"

namespace md {

SoundCPU::SoundCPU() {
    Z80::MemoryInterface mem{};

    auto readProg = [](u32 addr, void* ctx) -> u8 {
        Memory* m = static_cast<SoundCPU*>(ctx)->getMemory();
        return m ? m->readZ80(static_cast<u16>(addr)) : 0xFF;
    };
    mem.progRead  = readProg;
    mem.opRead    = readProg;
    mem.opArgRead = readProg;
    mem.progWrite = [](u32 addr, u8 val, void* ctx) {
        Memory* m = static_cast<SoundCPU*>(ctx)->getMemory();
        if (m) m->writeZ80(static_cast<u16>(addr), val);
    };
    // The Z80 I/O space is unused on the Mega Drive.
    mem.ioRead = [](u32, void*) -> u8 { return 0xFF; };
    mem.ioWrite = [](u32, u8, void*) {};
    mem.userData = this;

    m_z80.setMemory(mem);
}

SoundCPU::~SoundCPU() = default;

void SoundCPU::reset() {
    m_z80.reset();
    m_cycles = 0;
    m_busRequested = false;
    m_resetHeld = true;
}

void SoundCPU::setBusRequest(bool requested) {
    m_busRequested = requested;
}

void SoundCPU::setResetLine(bool held) {
    if (held && !m_resetHeld) {
        m_z80.reset();
    }
    m_resetHeld = held;
}

u32 SoundCPU::step(u32 cycles) {
    if (cycles == 0) return 0;

    // While the bus is granted to the 68000, or reset is asserted, the Z80 is
    // stopped but wall-clock time still passes.
    if (!isRunning()) {
        m_cycles += cycles;
        return cycles;
    }

    s32 executed = m_z80.execute(static_cast<s32>(cycles));
    if (executed < 0) executed = 0;
    m_cycles += static_cast<u32>(executed);
    return static_cast<u32>(executed);
}

void SoundCPU::irq(bool state) {
    m_z80.setIrqLine(0, state ? Z80::AssertLine : Z80::ClearLine);
}

void SoundCPU::saveState(Buffer* buf) {
    size_t ctxSize = Z80::contextSize();
    buffer_write(buf, &ctxSize, sizeof(ctxSize));

    Z80::State state;
    m_z80.getContext(&state);
    buffer_write(buf, &state, ctxSize);

    buffer_write(buf, &m_cycles, sizeof(m_cycles));
    buffer_write(buf, &m_busRequested, sizeof(m_busRequested));
    buffer_write(buf, &m_resetHeld, sizeof(m_resetHeld));
}

void SoundCPU::loadState(Buffer* buf) {
    size_t ctxSize = 0;
    buffer_read(buf, &ctxSize, sizeof(ctxSize));

    if (ctxSize != Z80::contextSize()) {
        log_error("Error: Saved Z80 context size mismatch");
        return;
    }

    Z80::State state;
    buffer_read(buf, &state, ctxSize);
    m_z80.setContext(&state);

    buffer_read(buf, &m_cycles, sizeof(m_cycles));
    buffer_read(buf, &m_busRequested, sizeof(m_busRequested));
    buffer_read(buf, &m_resetHeld, sizeof(m_resetHeld));
}

} // namespace md
