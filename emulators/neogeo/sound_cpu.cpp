#include "sound_cpu.h"
#include "memory.h"
#include "audio.h"
#include <cstring>
#include <vector>

namespace neogeo {

SoundCPU::SoundCPU()
    : m_memory(nullptr)
    , m_audio(nullptr)
    , m_cycles(0) {
    Z80::MemoryInterface mem{};
    auto readProg = [](u32 addr, void* ctx) -> u8 {
        Memory* m = static_cast<SoundCPU*>(ctx)->getMemory();
        return m ? m->readZ80(addr) : 0xFF;
    };
    mem.progRead  = readProg;
    mem.opRead    = readProg;
    mem.opArgRead = readProg;
    mem.progWrite = [](u32 addr, u8 val, void* ctx) {
        Memory* m = static_cast<SoundCPU*>(ctx)->getMemory();
        if (m) m->writeZ80(addr, val);
    };
    mem.ioRead = [](u32 port, void* ctx) -> u8 {
        Memory* m = static_cast<SoundCPU*>(ctx)->getMemory();
        return m ? m->readZ80IO(static_cast<u16>(port)) : 0xFF;
    };
    mem.ioWrite = [](u32 port, u8 val, void* ctx) {
        Memory* m = static_cast<SoundCPU*>(ctx)->getMemory();
        if (m) m->writeZ80IO(static_cast<u16>(port), val);
    };
    mem.userData = this;
    m_z80.setMemory(mem);
}

SoundCPU::~SoundCPU() = default;

void SoundCPU::reset() {
    m_z80.reset();
    m_cycles = 0;
}

u32 SoundCPU::step(u32 cycles) {
    if (cycles > 0) {
        s32 executed = m_z80.execute(static_cast<s32>(cycles));
        u32 actualCycles = static_cast<u32>(executed);
        m_cycles += actualCycles;

        // Update YM2610 timers - triggers IRQs when timers expire
        if (m_audio) {
            m_audio->updateTimers(actualCycles);
        }

        return actualCycles;
    }
    return 0;
}

void SoundCPU::irq(bool state) {
    m_z80.setIrqLine(0, state ? Z80::AssertLine : Z80::ClearLine);
}

void SoundCPU::nmi() {
    m_z80.setIrqLine(Z80::InputLineNmi, Z80::AssertLine);
    m_z80.setIrqLine(Z80::InputLineNmi, Z80::ClearLine);
}

void SoundCPU::saveState(Buffer* buf) {
    size_t ctxSize = Z80::contextSize();
    buffer_write(buf, &ctxSize, sizeof(ctxSize));

    Z80::State state;
    m_z80.getContext(&state);
    buffer_write(buf, &state, ctxSize);
    buffer_write(buf, &m_cycles, sizeof(m_cycles));
}

void SoundCPU::loadState(Buffer* buf) {
    size_t ctxSize;
    buffer_read(buf, &ctxSize, sizeof(ctxSize));

    if (ctxSize != Z80::contextSize()) {
        log_error("Error: Saved Z80 context size mismatch");
        return;
    }

    Z80::State state;
    buffer_read(buf, &state, ctxSize);
    m_z80.setContext(&state);

    buffer_read(buf, &m_cycles, sizeof(m_cycles));
}

} // namespace neogeo
