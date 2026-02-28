#include "sound_cpu.h"
#include "memory.h"
#include "audio.h"
#include "consts.h"
#include <cstring>

namespace cps {

// Static context pointer for callbacks
static SoundCPU* g_soundCpuContext = nullptr;

// Z80 callback functions
static u8 z80ReadProg(u32 address) {
    Memory* mem = g_soundCpuContext->getMemory();
    return mem ? mem->readZ80(address) : 0xFF;
}

static void z80WriteProg(u32 address, u8 value) {
    Memory* mem = g_soundCpuContext->getMemory();
    if (mem) mem->writeZ80(address, value);
}

static u8 z80ReadIo(u32 port) {
    Audio* audio = g_soundCpuContext->getAudio();
    return audio ? audio->readPort(port) : 0xFF;
}

static void z80WriteIo(u32 port, u8 value) {
    Audio* audio = g_soundCpuContext->getAudio();
    if (audio) audio->writePort(port, value);
}

static u8 z80ReadOp(u32 address) {
    Memory* mem = g_soundCpuContext->getMemory();
    return mem ? mem->readZ80Opcode(address) : 0xFF;
}

static u8 z80ReadOpArg(u32 address) {
    Memory* mem = g_soundCpuContext->getMemory();
    return mem ? mem->readZ80OpcodeArg(address) : 0xFF;
}

SoundCPU::SoundCPU()
    : m_memory(nullptr)
    , m_audio(nullptr)
    , m_cycles(0)
    , m_cartridge(nullptr)
    , m_cpsVersion(1)
    , m_timerAccumulator(0)
    , m_timerPeriod(0) {
    g_soundCpuContext = this;

    m_z80.setProgramReadHandler(z80ReadProg);
    m_z80.setProgramWriteHandler(z80WriteProg);
    m_z80.setIoReadHandler(z80ReadIo);
    m_z80.setIoWriteHandler(z80WriteIo);
    m_z80.setOpReadHandler(z80ReadOp);
    m_z80.setOpArgReadHandler(z80ReadOpArg);
}

SoundCPU::~SoundCPU() {
    if (g_soundCpuContext == this) {
        g_soundCpuContext = nullptr;
    }
}

void SoundCPU::reset() {
    m_z80.reset();
    m_cycles = 0;
    m_timerAccumulator = 0;

    if (m_cartridge->getCPSVersion() == 2) {
        m_cyclesPerFrame = ::cps2::SOUND_CPU_CYCLES_PER_FRAME;
    } else if (m_cartridge->isCPS1QSound()) {
        m_cyclesPerFrame = ::cps1qs::SOUND_CPU_CYCLES_PER_FRAME;
    } else {
        m_cyclesPerFrame = ::cps1::SOUND_CPU_CYCLES_PER_FRAME;
    }

    // Calculate timer period for CPS2 (252 Hz interrupt rate)
    if (m_cpsVersion == 2 || (m_cpsVersion == 1 && m_cartridge && m_cartridge->isCPS1QSound())) {
        if (m_cartridge->isCPS1QSound()) {
            m_timerPeriod = cps1qs::SOUND_CPU_FREQUENCY / 252;
        } else {
            m_timerPeriod = cps2::SOUND_CPU_FREQUENCY / 252;
        }
    } else {
        m_timerPeriod = 0;  // CPS1 uses YM2151 interrupts, not timer
    }
}

void SoundCPU::setCPSVersion(u8 version) {
    m_cpsVersion = version;
}

u32 SoundCPU::step(u32 cycles) {
    if (cycles > 0) {
        s32 executed = m_z80.execute(static_cast<s32>(cycles));
        u32 actualCycles = static_cast<u32>(executed);
        m_cycles += actualCycles;

        // For CPS2 and CPS1 QSound, check timer-based interrupt
        if (m_timerPeriod > 0) {
            m_timerAccumulator += executed;

            while (m_timerAccumulator >= m_timerPeriod) {
                m_timerAccumulator -= m_timerPeriod;
                m_z80.setIrqHold();
                m_z80.setIrqLine(0xFF, Z80::AssertLine);
            }
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
}

void SoundCPU::saveState(Buffer* buf) {
    Z80::Regs regs;
    m_z80.getContext(&regs);

    size_t ctxSize = Z80::contextSize();
    buffer_write(buf, &ctxSize, sizeof(ctxSize));
    buffer_write(buf, &regs, ctxSize);
    buffer_write(buf, &m_cycles, sizeof(m_cycles));
    buffer_write(buf, &m_cyclesPerFrame, sizeof(m_cyclesPerFrame));
    buffer_write(buf, &m_cpsVersion, sizeof(m_cpsVersion));
    buffer_write(buf, &m_timerAccumulator, sizeof(m_timerAccumulator));
    buffer_write(buf, &m_timerPeriod, sizeof(m_timerPeriod));
}

void SoundCPU::loadState(Buffer* buf) {
    size_t ctxSize;
    buffer_read(buf, &ctxSize, sizeof(ctxSize));

    if (ctxSize != Z80::contextSize()) {
        return;
    }

    Z80::Regs regs;
    buffer_read(buf, &regs, ctxSize);
    m_z80.setContext(&regs);

    buffer_read(buf, &m_cycles, sizeof(m_cycles));
    buffer_read(buf, &m_cyclesPerFrame, sizeof(m_cyclesPerFrame));
    buffer_read(buf, &m_cpsVersion, sizeof(m_cpsVersion));
    buffer_read(buf, &m_timerAccumulator, sizeof(m_timerAccumulator));
    buffer_read(buf, &m_timerPeriod, sizeof(m_timerPeriod));
}

} // namespace cps
